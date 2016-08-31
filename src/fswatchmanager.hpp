#pragma once

#include "fswatch.hpp"
#include "manager.hpp"
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::vector;
using std::string;

namespace fm {
  namespace land {
    class FSWatchManager {
      Manager &_manager;
      map<string, FSWatch> _watchers;

      void start_watching(const Replica &replica) {
        auto found = this->_watchers.find(replica.hash);
        if (found == this->_watchers.end()) {
          // We are not watching this replica yet
          auto watch = FSWatch{this->_manager, replica};
          auto result = this->_watchers.emplace(std::make_pair(replica.hash, std::move(watch)));
          if (std::get<1>(result)) {
            // We successfully added the replica to our map, let's start it up
            auto &new_fswatch = std::get<0>(result)->second;
            new_fswatch.start();
          }
        }
      }

      void stop_watching(const std::string &hash) {
        auto found = this->_watchers.find(hash);
        if (found != this->_watchers.end()) {
          std::get<1>(*found).stop();
        }
      }

    public:
      FSWatchManager(Manager &manager) : _manager(manager), _watchers() {
        // Whenever the manager starts watching a new replica, start a new FSWatch instance
        this->_manager.on_watch([this](const Replica &replica) {
          this->start_watching(replica);
        });

        this->_manager.on_unwatch([this](const Replica &replica) {
          this->stop_watching(replica.hash);
        });
      }

      void stop() {
        for (auto &watcher : this->_watchers) {
          auto &fswatcher = std::get<1>(watcher);
          fswatcher.stop();
        }
      }
    };
  }
}
