#ifndef _BENCHMARK_HPP_
#define _BENCHMARK_HPP_
#include "counters.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sched.h>

constexpr uint64_t BENCH_NUM = 300;
constexpr uint64_t WARM_UP_COOL_DOWN = 30;

template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench)
{
  cpu_set_t old_set;
  cpu_set_t new_set;

  CPU_ZERO(&new_set);
  CPU_SET(cpu, &new_set);
  sched_getaffinity(0, sizeof(cpu_set_t), &old_set);

  int ret = sched_setaffinity(0, sizeof(cpu_set_t), &new_set);
  if(ret != 0) exit(-10);

  fprintf(stderr, "%s\n", str);
  for(uint64_t i = 0; i < WARM_UP_COOL_DOWN *2 + BENCH_NUM; i++)
  {
    bench(count);
    if(WARM_UP_COOL_DOWN < i && WARM_UP_COOL_DOWN + BENCH_NUM > i)
      print_counters(count);
  }

  sched_setaffinity(0, sizeof(cpu_set_t), &old_set);
}

#endif
