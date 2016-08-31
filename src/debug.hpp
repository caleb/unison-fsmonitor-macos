#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include "../config.h"

#ifdef DEBUG
#define D(x) { x; }
#else
#define D(x) {}
#endif

namespace fm {
  namespace land {
    void log(std::string statement) {
      std::ofstream file;
      file.open("unison-fsmonitor.log", std::ios::app);
      file << statement << std::endl;
    }
  }
}
