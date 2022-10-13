#ifndef _BENCHMARK_HPP_
#define _BENCHMARK_HPP_
#include "counters.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sched.h>
#include <fstream>

constexpr uint64_t BENCH_NUM = 30;
constexpr uint64_t WARM_UP_COOL_DOWN = 5;
constexpr const char* statsFileName = "stats.txt";

template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench)
{
  std::ofstream stats(statsFileName, std::ofstream::app);
  if (!stats.is_open())
  {
    std::cerr << "FAILURE TO OPEN FILE" << statsFileName << std::endl;
    exit(-5);
  }
  cpu_set_t old_set;
  cpu_set_t new_set;

  CPU_ZERO(&new_set);
  CPU_SET(cpu, &new_set);
  sched_getaffinity(0, sizeof(cpu_set_t), &old_set);

  int ret = sched_setaffinity(0, sizeof(cpu_set_t), &new_set);
  if(ret != 0) exit(-10);

 // fprintf(stderr, "%s\t%" PRIu32 "\t%" PRIu64 "\n", str, num_counters(), BENCH_NUM);
  stats << str << "\t" << num_counters() << "\t" << BENCH_NUM << std::endl;
  for(uint64_t i = 0; i < WARM_UP_COOL_DOWN *2 + BENCH_NUM; i++)
  {
    bench(count);

    if(WARM_UP_COOL_DOWN <= i && WARM_UP_COOL_DOWN + BENCH_NUM > i)
      print_counters(count, stats);
  }

  sched_setaffinity(0, sizeof(cpu_set_t), &old_set);
}

#endif
