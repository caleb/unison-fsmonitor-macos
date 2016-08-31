#include <libfswatch/c++/event.hpp>
#include <libfswatch/c++/filter.hpp>
#include <libfswatch/c++/monitor.hpp>

#include "manager.hpp"
#include <cstdio>
#include <memory>
#include <sstream>
#include <thread>

using std::unique_ptr;

namespace fm {
  namespace land {

    struct Context {
      Manager &manager;
      const Replica &replica;

      Context(Manager &manager, const Replica &replica) : manager(manager), replica(replica) {}
    };

    class FSWatch {
      unique_ptr<fsw::monitor> _monitor;
      Manager &_manager;
      std::thread _thread;
      const Replica &_replica;

      Context *_context;

    public:
      FSWatch(FSWatch &&watch) : _monitor{std::move(watch._monitor)},
                                 _manager{watch._manager},
                                 _replica{watch._replica},
                                 _context{watch._context},
                                 _thread{std::move(watch._thread)} {
        watch._context = nullptr;
      }

      FSWatch(Manager &manager, const Replica &replica) : _monitor{}, _manager{manager}, _replica{replica} {
        this->_context = new Context{this->_manager, this->_replica};
        this->_monitor.reset(fsw::monitor_factory::create_monitor(fsw_monitor_type::system_default_monitor_type,
                                                                  {replica.fspath},
                                                                  [](const std::vector<fsw::event> &events, void *context) {
                                                                    Context *ctx = static_cast<Context *>(context);
                                                                    ctx->manager.push_fs_events(ctx->replica, events);
                                                                  },
                                                                  static_cast<void *>(this->_context)));

        // Don't watch .git and .hg folders
        this->_monitor->add_filter(this->create_filter(fsw_filter_type::filter_exclude, "\\.git"));
        this->_monitor->add_filter(this->create_filter(fsw_filter_type::filter_exclude, "\\.hg"));

        this->_monitor->set_directory_only(true);
      }

      fsw::monitor_filter create_filter(fsw_filter_type type, std::string text) {
        fsw::monitor_filter filter;
        filter.type = type;
        filter.text = text;
        filter.extended = true;
        return filter;
      }

      void process_events(const std::vector<fsw::event> &events) {
        this->_manager.push_fs_events(this->_replica, events);
      }

      bool is_running() const {
        return this->_monitor->is_running();
      }

      void start() {
        if (!this->_monitor->is_running()) {
          this->_thread = std::thread([this]() {
            this->_monitor->start();
          });
        }
      }
      void stop() {
        if (this->_monitor) {
          if (this->_monitor->is_running()) {
            this->_monitor->stop();
          }
        }
      }

      ~FSWatch() {
        if (this->_context) {
          delete this->_context;
        }

        if (this->_monitor) {
          if (this->_monitor->is_running()) {
            this->_monitor->stop();
          }
        }
      }
    };
  }
}
