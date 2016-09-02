#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <list>
#include <vector>

#include <boost/filesystem.hpp>
#include <libfswatch/c++/event.hpp>

#include "directory.hpp"
#include "plf_colony.h"
#include "group_by.hpp"
#include "utility.hpp"

using std::queue;
using std::map;
using std::string;
using std::tuple;
using std::vector;
using std::list;
using std::mutex;
using std::lock_guard;
using std::set;
using std::function;

using boost::filesystem::path;

namespace fm {
  namespace land {
    struct Replica {
      string hash;
      string fspath;
      set<string> paths;

      Replica() : hash(""), fspath(""), paths() {}
      Replica(const Replica &replica) : hash(replica.hash), fspath(replica.fspath), paths(replica.paths) {}
      Replica(Replica &&replica) noexcept : hash(std::move(replica.hash)), fspath(std::move(replica.fspath)), paths(std::move(replica.paths)) {}
      Replica(string hash, string fspath) : hash(hash), fspath(fspath) {}
      Replica(string hash, string fspath, set<string> paths) : hash(hash), fspath(fspath), paths(paths) {}

      ~Replica() {}

      Replica &operator=(const Replica &replica) {
        this->hash = replica.hash;
        this->fspath = replica.fspath;
        this->paths = replica.paths;

        return *this;
      }

      void add_path(const string &path) {
        this->paths.insert(path);
      }

      void merge(const Replica &replica) {
        if (this->hash == replica.hash) {
          for (auto &path : replica.paths) {
            this->paths.insert(path);
          }
        }
      }
    };

    class Manager {
      using watch_listener_t = function<void(const Replica &)>;
      using fs_change_listener_t = function<void(const string &)>;

      mutex fs_changes_mutex;
      mutex watch_listeners_mutex;
      plf::colony<Replica> _replicas;
      vector<watch_listener_t> _watch_listeners;
      vector<watch_listener_t> _off_watch_listeners;
      map<string, Directory> _directory;

      vector<fs_change_listener_t> _fs_change_listeners;

    public:
      Manager() {}

      /*
       * Add a replica to our collection
       */
      void add_replica(Replica replica) {
        auto rep = std::find_if(this->_replicas.begin(), this->_replicas.end(), [&replica](const Replica &candidate_replica) {
          return candidate_replica.hash == replica.hash;
        });

        if (rep == this->_replicas.end()) {
          auto result = this->_replicas.insert(std::move(replica));
          Replica &new_replica = *result;
          // Invoke the listeners
          for (auto &listener : this->_watch_listeners) {
            listener(new_replica);
          }
        } else {
          rep->merge(replica);
        }
      }

      void on_watch(watch_listener_t listener) {
        lock_guard<mutex> guard(this->watch_listeners_mutex);
        this->_watch_listeners.push_back(listener);
      }

      void on_unwatch(watch_listener_t listener) {
        lock_guard<mutex> guard(this->watch_listeners_mutex);
        this->_off_watch_listeners.push_back(listener);
      }

      void on_fs_change(fs_change_listener_t listener) {
        lock_guard<mutex> guard(this->watch_listeners_mutex);
        this->_fs_change_listeners.push_back(listener);
      }

      const plf::colony<Replica> &replicas() const {
        return this->_replicas;
      }

      bool has_replica(const string &hash) {
        return this->replica(hash).isOk();
      }

      Result<std::reference_wrapper<const Replica>> replica(const string &hash) {
        for (const auto &replica : this->_replicas) {
          if (replica.hash == hash) {
            return Ok(std::cref(replica));
          }
        }

        return Err("No replica found with hash " + hash);
      }

      void trigger_change(const Replica &replica) {
        for (auto &listener : this->_fs_change_listeners) {
          listener(replica.hash);
        }
      }

      void push_fs_events(const Replica &replica, const vector<fsw::event> &events) {
        // Ensure we release the guard before triggering change handlers so they can invoke
        // methods that require a lock
        {
          lock_guard<mutex> guard{this->fs_changes_mutex};
          path fspath(replica.fspath);

          for (auto &e : events) {
            auto *dir = &this->_directory[replica.hash];
            path p(e.get_path());

            path rel = p.lexically_relative(fspath);

            if (!rel.has_parent_path()) {
              // The root has changed
              dir->terminate();
            } else {
              path parent = rel.parent_path();

              for (auto &comp : parent) {
                dir = &dir->child(comp.string());
              }

              dir->terminate();
            }
          }
        }

        this->trigger_change(replica);
      }

      Directory &directory(const string &hash) {
        lock_guard<mutex> guard{this->fs_changes_mutex};
        return this->_directory[hash];
      }

      Directory consume_directory(const string &hash) {
        lock_guard<mutex> guard{this->fs_changes_mutex};

        Directory d = this->_directory[hash];
        this->_directory.erase(hash);
        return d;
      }

      vector<string> changed_replicas(const vector<string> &interested_hashes) {
        vector<string> changed_hashes;
        lock_guard<mutex> guard{this->fs_changes_mutex};

        for (auto &kv : this->_directory) {
          if (kv.second.has_changes() && std::find(interested_hashes.begin(), interested_hashes.end(), kv.first) != interested_hashes.end()) {
            changed_hashes.push_back(kv.first);
          }
        }

        return changed_hashes;
      }
    };
  }
}
