#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../config.h"
#include "debug.hpp"
#include "directory.hpp"
#include "fswatchmanager.hpp"
#include "glib.h"
#include "manager.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

using std::string;
using std::vector;
using std::thread;
using std::mutex;
using std::lock_guard;

using boost::filesystem::path;
using boost::filesystem::directory_iterator;
using boost::filesystem::recursive_directory_iterator;

namespace fm {
  namespace land {
    class UnisonManager {
      Manager &_manager;
      set<string> _waiting;
      mutex _waiting_mutex;
      mutex _stdout_mutex;

    public:
      UnisonManager(Manager &manager);
      void send(const string &command, const vector<string> &args);
      void ack();
      Manager &manager();
      void start();
      bool is_waiting(const string &hash);
      vector<string> waiting();
      void wait(const string &hash);
      void clear_waiting();
    };

    std::string urlencode(const std::string &s) {
      char *c_result = g_uri_escape_string(s.c_str(), "/", false);
      std::string result(c_result);
      g_free(c_result);
      return result;
    }

    std::string urldecode(const std::string &s) {
      char *c_result = g_uri_unescape_string(s.c_str(), nullptr);
      std::string result(c_result);
      g_free(c_result);
      return result;
    }

    vector<string> process_args(const string &input) {
      vector<string> result;
      boost::split(result, input, boost::is_any_of("\t "));
      vector<string> transformed_result(result.size());

      std::transform(std::make_move_iterator(result.begin()),
                     std::make_move_iterator(result.end()),
                     transformed_result.begin(), [](const string &s) {
                       return urldecode(s);
                     });

      return transformed_result;
    }

    string receive() {
      string input;
      std::getline(std::cin, input);
      boost::trim(input);
      D(log(">>> Received \"" + input + "\""));
      return input;
    }

    class Command {
      UnisonManager &_unison_manager;

    public:
      Command(UnisonManager &unison_manager) : _unison_manager{unison_manager} {}

      Manager &manager() {
        return this->_unison_manager.manager();
      }

      void send(const string &command, const vector<string> &args) {
        this->_unison_manager.send(command, args);
      }
      void ack() {
        this->_unison_manager.ack();
      }
    };

    class ChangesCommand : Command {
    public:
      ChangesCommand(UnisonManager &unison_manager) : Command{unison_manager} {}

      void process(const vector<string> &args) {
        string hash = args[0];
        Directory dir = this->manager().consume_directory(hash);

        this->send_recursive(path("."), dir);

        this->send("DONE", {});
      }

      void send_recursive(const path &p, const Directory &dir) {
        if (dir.terminated()) {
          this->send("RECURSIVE", {p.string()});
        } else {
          dir.each_child([this, &p](const string &comp, const Directory &dir2) {
            this->send_recursive(p / path(comp), dir2);
          });
        }
      }
    };

    class StartCommand : Command {
    public:
      StartCommand(UnisonManager &unison_manager) : Command{unison_manager} {}

      void process(const vector<string> &args) {
        string hash = args[0];
        string fspath = args[1];
        string path;

        if (!this->manager().has_replica(hash)) {
          // Add the replica to the manager
          if (args.size() == 3) {
            this->manager().add_replica({hash, fspath, {path}});
          } else if (args.size() == 2) {
            this->manager().add_replica({hash, fspath});
          }
        }

        this->ack();

        string input;
        vector<string> command_words;
        string command;

        while (true) {
          input = receive();

          command_words = process_args(input);
          command = command_words[0];

          if (command == "DONE") {
            break;
          } else if (command == "DIR") {
            this->ack();
          } else if (command == "LINK") {
            this->ack();
          }
        }
      }
    };

    UnisonManager::UnisonManager(Manager &manager) : _manager{manager} {
      manager.on_fs_change([this](const string &hash) {
        if (this->is_waiting(hash)) {
          auto changed = this->_manager.changed_replicas(this->waiting());
          this->clear_waiting();
          this->send("CHANGES", changed);
        }
      });
    }

    void UnisonManager::send(const string &command, const vector<string> &args) {
      vector<string> encoded_args(args.size());

      std::transform(args.begin(),
                     args.end(),
                     encoded_args.begin(), [](const string &arg) {
                       return urlencode(arg);
                     });

      string command_string = std::accumulate(encoded_args.begin(),
                                              encoded_args.end(),
                                              command,
                                              [](const string &arg1, const string &arg2) {
                                                return arg1 + " " + arg2;
                                              });

      std::lock_guard<std::mutex> lock(this->_stdout_mutex);
      std::cout << command_string << std::endl;
      std::cout.flush();
      D(log("<<< Sent \"" + command_string + "\""));
    }
    void UnisonManager::ack() {
      this->send("OK", {});
    }

    Manager &UnisonManager::manager() {
      return this->_manager;
    }

    bool UnisonManager::is_waiting(const string &hash) {
      lock_guard<mutex> lock(this->_waiting_mutex);
      return this->_waiting.find(hash) != this->_waiting.end();
    }

    void UnisonManager::wait(const string &hash) {
      lock_guard<mutex> lock(this->_waiting_mutex);
      this->_waiting.insert(hash);
    }
    vector<string> UnisonManager::waiting() {
      lock_guard<mutex> lock(this->_waiting_mutex);
      return {this->_waiting.begin(), this->_waiting.end()};
    }
    void UnisonManager::clear_waiting() {
      lock_guard<mutex> lock(this->_waiting_mutex);
      this->_waiting.clear();
    }

    /*
        When Unison start scanning a part of the replica, it emits command:
        'START hash fspath path', thus indicating the archive hash (that
        uniquely determines the replica) the replica's fspath and the path
        where the scanning process starts. The child process should start
        monitoring this location, then acknowledge the command by sending an
        'OK' response.

        When Unison starts scanning a directory, it emits the command
        'DIR path1', where 'path1' is relative to the path given by the
        START command (the location of the directory can be obtained by
        concatenation of 'fspath', 'path', and 'path1'). The child process
        should then start monitoring the directory, before sending an 'OK'
        response.

        When Unison encounters a followed link, it emits the command
        'LINK path1'. The child process is expected to start monitoring
        the link target before replying by 'OK'.

        Unison signals that it is done scanning the part of the replica
        described by the START process by emitting the 'DONE' command. The
        child process should not respond to this command.

        Unison can ask for a list of paths containing changes in a given
        replica by sending the 'CHANGES hash' command. The child process
        responds by a sequence of 'RECURSIVE path' responses, followed by a
        'DONE' response. These paths should be relative to the replica
        'fspath'. The child process will not have to report this changes any
        more: it can consider that Unison has taken this information into
        account once and for all. Thus, it is expected to thereafter report
        only further changes.

        Unison can wait for changes in a replica by emitting a 'WAIT hash'
        command. It can watch several replicas by sending a serie of these
        commands. The child process is expected to respond once, by a
        'CHANGE hash1 ... hash2' response that lists the changed replicas
        among those included in a 'WAIT' command, when changes are
        available. It should cancel pending waits when any other command is
        received.

        Finally, the command 'RESET hash' tells the child process to stop
        watching the given replica. In particular, it can discard any
        pending change information for this replica.
       */

    void UnisonManager::start() {
      string input;
      string command;
      vector<string> command_words;
      vector<string> args;

      // Output our version
      this->send("VERSION", {"1"});

      while (true) {
        input = receive();

        command_words = process_args(input);
        if (command_words.size() == 0) {
          continue;
        }

        // Grab the command
        command = command_words[0];

        if (command_words.size() > 1) {
          args = vector<string>(std::make_move_iterator(++command_words.begin()),
                                std::make_move_iterator(command_words.end()));
        } else {
          args = {};
        }

        // If we receive a command other than a wait command, clear our waiting set
        if (command != "WAIT") {
          this->clear_waiting();
        }

        if (command == "START") {
          StartCommand(*this).process(args);
        } else if (command == "CHANGES") {
          ChangesCommand(*this).process(args);
        } else if (command == "WAIT") {
          string hash = args[0];

          auto changed = this->_manager.changed_replicas(this->waiting());
          if (changed.size() > 0) {
            this->send("CHANGES", changed);
          } else {
            this->wait(hash);
          }
        } else if (command == "RESET") {
        }
      }
    };
  }
}
