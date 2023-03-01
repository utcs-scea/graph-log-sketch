#ifndef _BENCHMARK_HPP_
#define _BENCHMARK_HPP_
#include <counters.hpp>
#include <sched.h>
#include <fstream>
#include <cerrno>
#include <string>
#include <system_error>
#include <iostream>

#include <random>
#include <algorithm>

constexpr uint64_t rseed = 48048593;

constexpr uint64_t BENCH_NUM = 1;
constexpr uint64_t WARM_UP_COOL_DOWN = 0;

struct RNG
{
  uint64_t operator() (uint64_t n){ return std::rand() % n; }
};

template<typename R>
void runner(std::string s, R r)
{
  r.template operator()<false, false>(s);
  r.template operator()<false, true >(s + " EHP");
  r.template operator()<true , false>(s + " PAR");
  r.template operator()<true , true >(s + " PAR EHP");
}

#ifdef BENCH
template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench, const char* statsFileName = "stats.txt")
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

template<typename B>
void benchmark(pa count, const char* str, B bench, const char* statsFileName = "stats.txt")
{
  std::ofstream stats(statsFileName, std::ofstream::app);
  if (!stats.is_open())
  {
    std::cerr << "FAILURE TO OPEN FILE " << statsFileName << std::endl;
    exit(-5);
  }

  stats << str << "\t" << num_counters() << "\t" << BENCH_NUM << std::endl;
  for(uint64_t i = 0; i < WARM_UP_COOL_DOWN *2 + BENCH_NUM; i++)
  {
    bench(count);

    if(WARM_UP_COOL_DOWN <= i && WARM_UP_COOL_DOWN + BENCH_NUM > i)
      print_counters(count, stats);
  }

}

#else
template<typename B>
void benchmark(pa count, const char* str, int cpu, B bench, const char* statsFileName = "stats.txt")
{
  bench(count);
}

template<typename B>
void benchmark(pa count, const char* str, B bench, const char* statsFileName = "stats.txt")
{
  bench(count);
}
#endif

template<typename T, typename S, typename C, typename B>
void run_benchmark(const std::string& statsfn, std::string bench_name, pa count, int cpu, S setup, B bench, C cleanup)
{
  auto b = [&setup, &bench, &cleanup](pa c)
  {
    T t = setup();
    reset_counters(c);
    start_counters(c);
    bench(t);
    stop_counters(c);
    cleanup(t);
  };
  benchmark(count, bench_name.c_str(), cpu, b, statsfn.c_str());
}

template<typename T, typename S, typename C, typename B>
void run_benchmark(const std::string& statsfn, std::string bench_name, pa count, S setup, B bench, C cleanup)
{
  auto b = [&setup, &bench, &cleanup](pa c)
  {
    T t = setup();
    reset_counters(c);
    start_counters(c);
    bench(t);
    stop_counters(c);
    cleanup(t);
  };
  benchmark(count, bench_name.c_str(), b, statsfn.c_str());
}

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

std::vector<uint64_t> rand_nodes(uint64_t sz, uint64_t num_nodes, uint64_t rseed_vec)
{
  std::vector<uint64_t> samps;
  for(uint64_t i = 0; i < num_nodes; i++) samps.push_back(i);
  srand(rseed_vec);
  std::random_shuffle(samps.begin(), samps.end(), RNG());
  samps.resize(sz);
  std::sort(samps.begin(), samps.end());
  return samps;
}

std::vector<std::pair<uint64_t,uint64_t>> el_file_to_rand_vec_edge(const std::string& ELFile, uint64_t& num_nodes, uint64_t& num_edges)
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

  std::vector<std::pair<uint64_t,uint64_t>> ret;

  while(graphFile >> src && graphFile >> dest)
  {
    ret.emplace_back(src,dest);
  }

  std::srand(rseed);
  std::random_shuffle(ret.begin(), ret.end(), RNG());

  num_edges = ret.size();

  graphFile.close();

  return ret;
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
