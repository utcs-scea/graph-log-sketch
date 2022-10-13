#ifndef _READ_FILE_BENCH_HPP_
#define _READ_FILE_BENCH_HPP_
#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>

void check_el_file_and_benchmark(Graph* g, pa c, std::string ELFile)
{
    std::string cmd = "head -n 1 " + ELFile + " && tail -n +2 " + ELFile + " | sort -t' ' -k1,1n -k2,2n";
    FILE* fpG = popen(cmd.c_str(), "r");

    g->ingestSubGraphFromELFile(*&ELFile);

    auto fscanf_help = [fpG] (uint64_t& fst, uint64_t& snd) {
      return fscanf(fpG, "%" PRIu64 " %" PRIu64 "\n", &fst, &snd);
    };

    uint64_t num_nodes;
    uint64_t useless;
    int num = fscanf_help(num_nodes, useless);

    REQUIRE( num == 2 );

    REQUIRE( g->get_num_nodes() == num_nodes );

    uint64_t dest;
    uint64_t src;
    uint64_t count = 0;

    while(2 == fscanf_help(src, dest))
    {
      REQUIRE( count < g->get_edge_end() );
      REQUIRE( g->get_edge(count) != nullptr );
      REQUIRE( g->get_edge(count)->get_dest() == dest );
      REQUIRE( g->get_edge(count)->is_tomb() == false );
      REQUIRE( src < g->get_num_nodes() );
      REQUIRE( g->get_node(src) != nullptr );
      REQUIRE( g->get_node(src)->is_tomb() == false );
      REQUIRE( g->get_node(src)->start.get_value() <= count );
      REQUIRE( g->get_node(src)->stop > count );
      count++;
    }

    REQUIRE( count == g->get_edge_end() );

    pclose(fpG);
    std::string bench_name = "Sequentially Ingest Graph from " + ELFile;

    benchmark(c, bench_name.c_str(), 7, [ELFile](pa c)
    {
      Graph* p = new Graph();
      reset_counters(c);
      start_counters(c);
      p->ingestSubGraphFromELFile(*&ELFile);
      stop_counters(c);
      delete p;
    });
}
#endif