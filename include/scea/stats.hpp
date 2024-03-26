// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <sys/resource.h>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <string>

class HighResTimer {
private:
  std::chrono::high_resolution_clock::time_point start_time;
  std::chrono::high_resolution_clock::time_point end_time;

public:
  HighResTimer() = default;

  void start() { start_time = std::chrono::high_resolution_clock::now(); }

  void stop() { end_time = std::chrono::high_resolution_clock::now(); }

  uint64_t getDurationNano() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time -
                                                                start_time)
        .count();
  }
};

class ScopeBenchmarker {
private:
  HighResTimer timer;
  std::string scopeName;

  static uint64_t getMaxRSS() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#if defined(__linux__)
    return usage.ru_maxrss;
#else
    return usage.ru_maxrss * (sysconf(_SC_PAGESIZE) / 1024);
#endif
  }

public:
  explicit ScopeBenchmarker(const std::string& name) : scopeName(name) {
    timer.start();
  }

  ~ScopeBenchmarker() {
    timer.stop();
    uint64_t max_rss = getMaxRSS();

    std::cout << "Benchmark results for " << scopeName << ":\n";
    std::cout << "Duration: " << timer.getDurationNano() << " nanoseconds\n";
    std::cout << "Max RSS: " << max_rss << " KB\n";
  }
};

#define BENCHMARK_SCOPE(name) ScopeBenchmarker benchmarker##__LINE__(name)
