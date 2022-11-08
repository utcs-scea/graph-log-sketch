#ifndef _BENCHMARK_HPP_
#define _BENCHMARK_HPP_
#include <counters.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sched.h>
#include <fstream>
#include <cerrno>
#include <string>
#include <system_error>
#include <iostream>

constexpr uint64_t BENCH_NUM = 30;
constexpr uint64_t WARM_UP_COOL_DOWN = 5;
constexpr const char* statsFileName = "stats.txt";

#ifdef BENCH
template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench)
{
  std::ofstream stats(statsFileName, std::ofstream::app);
  if (!stats.is_open())
  {
    std::cerr << "FAILURE TO OPEN FILE " << statsFileName << std::endl;
    exit(-5);
  }
  cpu_set_t old_set;
  cpu_set_t new_set;

  CPU_ZERO(&new_set);
  CPU_SET(cpu, &new_set);
  sched_getaffinity(0, sizeof(cpu_set_t), &old_set);

  int ret = sched_setaffinity(0, sizeof(cpu_set_t), &new_set);
  if(ret != 0)
  {
    std::cerr << "UNABLE TO PROPERLY SET SCHEDULER AFFINITY ret_code: " << ret
              << "\terrno: " << errno
              << "\terrstr: " << strerror(errno) << std::endl;
    exit(-10);
  }

 // fprintf(stderr, "%s\t%" PRIu32 "\t%" PRIu64 "\n", str, num_counters(), BENCH_NUM);
  stats << str << "\t" << num_counters() << "\t" << BENCH_NUM << std::endl;
  for(uint64_t i = 0; i < WARM_UP_COOL_DOWN *2 + BENCH_NUM; i++)
  {
    bench(count);

    if(WARM_UP_COOL_DOWN <= i && WARM_UP_COOL_DOWN + BENCH_NUM > i)
      print_counters(count, stats);
  }

  ret = sched_setaffinity(0, sizeof(cpu_set_t), &old_set);
  if (ret != 0)
  {
    std::cerr << "UNABLE TO PROPERLY SET SCHEDULER AFFINITY ret_code: " << ret
              << "\terrno: " << errno
              << "\terrstr: " << strerror(errno) << std::endl;
    exit(-11);
  }

}
#else
template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench) {}
#endif

template<typename T, typename S, typename C, typename A, typename B>
void run_test_and_benchmark(std::string bench_name, pa count, int cpu, S setup, B bench, A asserts, C cleanup)
{
  T t = setup();
  bench(t);
  asserts(t);
  cleanup(t);
  auto b = [&setup, &bench, &cleanup](pa c)
  {
    T t = setup();
    reset_counters(c);
    start_counters(c);
    bench(t);
    stop_counters(c);
    cleanup(t);
  };
  benchmark(count, bench_name.c_str(), cpu, b);
}

std::vector<uint64_t>* el_file_to_edge_list(const std::string& ELFile, uint64_t& num_nodes, uint64_t& num_edges)
{
  std::ifstream graphFile(ELFile.c_str());
  if (!graphFile.is_open())
  {
    std::cerr << "UNABLE TO open graphFile: " << ELFile
              << "\terrno: " << errno
              << "\terrstr: " << strerror(errno)
              << std::endl;

    exit(-2);
  }

  uint64_t src;
  uint64_t dest;

  graphFile >> num_nodes;
  graphFile >> src;

  std::vector<uint64_t>* ret = new std::vector<uint64_t>[num_nodes];

  num_edges = 0;

  while(graphFile >> src && graphFile >> dest)
  {
    num_edges++;
    ret[src].emplace_back(dest);
  }

  graphFile.close();
  return ret;
}

#endif
