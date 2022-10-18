#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <test_bench.hpp>

TEST_CASE( "Generate Large Graphs" )
{
  pa c = create_counters();

  SECTION( "Ingest Yelp Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/yelp.el");
  }

}
