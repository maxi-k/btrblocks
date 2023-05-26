#pragma once
#include <cstring>
#include <iostream>

/**
 * Use an external perf process to profile part of the program.
 * Use together with the 'perf-partial' shell script.
 * Can only be used once in a program together with the shell script.
 * */
struct PerfExternal {
  static void start(bool print = false) {
    std::string input;
    std::cout << "perf_point_1" << std::endl;
    std::getline(std::cin, input);
    if (print) {
      std::cout << "perf point 1 done, read" << input << "/" << input.size() << std::endl;
    }
  }

  static void stop(bool print = false) {
    std::string input;
    std::cout << "perf_point_2" << std::endl;
    std::getline(std::cin, input);
    if (print) {
      std::cout << "perf point 2 done, read " << input << "/" << input.size() << std::endl;
    }
  }
};

struct PerfExternalBlock {
  bool activate = true;

  explicit PerfExternalBlock(bool activate) : activate(activate) { start(); }
  PerfExternalBlock() {
    char* e = getenv("PERF");
    activate = e != nullptr && (!strcmp(e, "true") || !strcmp(e, "t") || !strcmp(e, "y") ||
                                !strcmp(e, "yes") || !strcmp(e, "1"));
    start();
  }

  ~PerfExternalBlock() { stop(); }

 private:
  void start() {
    if (activate) {
      PerfExternal::start(false);
    }
  }
  void stop() {
    if (activate) {
      PerfExternal::stop(false);
    }
  }
};
