#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

using std::map;
using std::string;
using std::function;

namespace fm {
  namespace land {
    class Directory {
      bool _is_terminated;
      bool _has_changes;
      map<string, Directory> _contents;

    public:
      Directory() : _is_terminated(false), _has_changes(false), _contents() {}

      void each_child(const function<void(const string &, const Directory &)> &f) const {
        for (auto &kv : this->_contents) {
          f(kv.first, kv.second);
        }
      }

      bool has_changes() {
        return this->_has_changes;
      }

      Directory &child(const string &path) {
        auto &child = this->_contents[path];
        this->_has_changes = true;
        return child;
      }

      void terminate() {
        this->_has_changes = true;
        this->_is_terminated = true;
      }
      bool terminated() const {
        return this->_is_terminated;
      }
    };
  }
}
