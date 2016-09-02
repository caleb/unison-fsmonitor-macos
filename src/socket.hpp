#pragma once

#include "json.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "result.hpp"
#include <iostream>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/stream.hpp>

namespace fm {
  namespace land {
    using json = nlohmann::json;
    namespace io = boost::iostreams;

    result<int> connect_to_socket(std::string path) {
      sockaddr_un sa;
      memset(&sa, 0, sizeof(sa));
      sa.sun_family = AF_UNIX;
      memcpy(sa.sun_path, path.c_str(), path.length());

      int fd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (fd < 0) {
        return err("Error creating socket");
      }

      if (connect(fd, (sockaddr *) &sa, sizeof(sa)) < 0) {
        return err("Error creating socket");
      }

      return ok(fd);
    }

    result<size_t> send(int fd, const char* req) {
      size_t s = strlen(req);
      size_t written = 0;

      while (true) {
        written += write(fd, req, s - written);
        if (written == s) { break; }
      }

      return ok(written);
    }

    result<std::string> recv(int fd) {
      io::file_descriptor_source src(fd, io::never_close_handle);
      io::stream_buffer<io::file_descriptor_source> fpstream(src);
      std::istream in(&fpstream);

      std::stringstream sstream;
      std::array<char, 512> buf;
      in.getline(&buf.front(), 512);
      sstream << &buf.front();
      while (in.fail() && ! in.eof()) {
        in.clear();
        in.getline(&buf.front(), 512);
        sstream << &buf.front();
      }

      if (in.fail() || in.eof()) {
        return err("Failed to extract response from watchman");
      }

      return ok(sstream.str());
    }
  }
}
