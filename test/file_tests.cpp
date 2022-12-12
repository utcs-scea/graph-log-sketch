#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <test_bench.hpp>

TEST_CASE( "Generating Small Graphs", "[files]")
{
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  SECTION( "Ingest Citeseer Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/citeseer.el");
  }

  SECTION( "Ingest Cora Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/cora.el");
  }
}

TEST_CASE( "Generate Large Graphs", "[files]")
{
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  SECTION( "Ingest Yelp Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/yelp.el");
  }
}
