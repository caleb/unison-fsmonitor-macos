#pragma once

#include "json.hpp"
#include "result.hpp"
#include "socket.hpp"
#include "watchman.hpp"

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <iostream>

namespace fm {
  namespace land {
    class Watchman {
      int _fd;
      std::string _sockname;

      result<std::string> find_socket() {
        pid_t pid;
        int pipefd[2];
        int meta_pipe[2];

        if (pipe(pipefd) < 0) {
          return err("Could not create pipe");
        }
        if (pipe(meta_pipe) < 0) {
          return err("Could not create pipe");
        }

        pid = fork();
        if (pid < 0) {
          return err("Could not fork");
        } else if (pid == 0) {
          // Child process
          close(pipefd[0]);

          // Redirect standard output to the pipe
          if (dup2(pipefd[1], 1) < 0) {
            const char* message = "Could not dup2 pipe";
            write(meta_pipe[1], message, strlen(message) + 1);
            close(pipefd[1]);
            exit(-1);
          }

          // Redirect standard error to /dev/null
          int dev_null_fd = open("/dev/null", O_RDWR);
          if (dev_null_fd < 0) {
            const char* message = "Could not open /dev/null";
            write(meta_pipe[1], message, strlen(message) + 1);
            close(pipefd[1]);
            exit(-1);
          }

          if (dup2(dev_null_fd, 2) < 0) {
            const char* message = "Could not dup2 pipe";
            write(meta_pipe[1], message, strlen(message) + 1);
            close(pipefd[1]);
            exit(-1);
          }

          // Everything looks good, lets call watchman
          execlp("watchman", "watchman", "get-sockname", (char* ) NULL);
          const char* message = "Could not execlp watchman";
          write(meta_pipe[1], message, strlen(message) + 1);
          close(pipefd[1]);
          exit(-1);
        } else {
          //parent process
          close(pipefd[1]);
          close(meta_pipe[1]);
          int fd = pipefd[0];
          char buf[255];
          std::stringstream result;
          std::stringstream errorstream;
          std::string errors;

          // Read the output from watchman
          size_t bytes_read;
          while (true) {
            bytes_read = read(fd, buf, 254);
            if (bytes_read == 0) break;

            buf[bytes_read] = 0;
            result << buf;
          };

          // Read the output from the error pipe
          while (true) {
            bytes_read = read(meta_pipe[0], buf, 254);
            if (bytes_read == 0) break;

            buf[bytes_read] = 0;
            errorstream << buf;
          }
          errors = errorstream.str();

          if (errors.length() > 0) {
            return err(std::move(errors));
          } else {
            return ok(result.str());
          }
        }
      }

    public:
      Watchman() {
      }

      result<void> connect() {
        using json = nlohmann::json;

        auto result = this->find_socket();
        if (! result) { return result.err(); }

        json j = json::parse(result.ok());
        std::string sockname;

        if (j.find("sockname") == j.end()) {
          return err("Error parsing watchman output: \n\n" + result.ok() + "\nCould not find key \"sockname\"");
        }

        if (! j["sockname"].is_string()) {
          return err("sockname isn't string: \n\n" + result.ok());
        }

        this->_sockname = j["sockname"];

        auto socket = connect_to_socket(this->_sockname);
        if (! socket) { return socket.err(); }

        this->_fd = socket.ok();

        return ok();
      }
      result<std::string> watch_project(std::string path) {
        using json = nlohmann::json;
        json req = {"watch-project", path};
        std::string sreq(req.dump());
        sreq.push_back('\n');

        std::cout << "Sending request: " << sreq << std::endl;

        auto send_res = send(this->_fd, sreq.c_str());
        if (! send_res) {
          return err("Could not receive response from watchman");
        }

        auto res = recv(this->_fd);

        if (! res) {
          return err("Could not receive response from watchman");
        }

        json json_res = res.ok();

        std::cout << "Got response " << json_res.dump() << std::endl;

        return ok(std::string("Hello world"));
      }
    };
  }
}
