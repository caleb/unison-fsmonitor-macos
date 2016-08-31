#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "fswatchmanager.hpp"
#include "manager.hpp"
#include "unisonmanager.hpp"

int main(int argc, char **argv) {
  using namespace fm::land;

  Manager manager;
  FSWatchManager fswatch_manager{manager};
  UnisonManager unison_manager{manager};

  unison_manager.start();

  // When we quit, stop our watchers
  fswatch_manager.stop();

  return 0;
}
