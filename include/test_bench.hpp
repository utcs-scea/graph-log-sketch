#ifndef _READ_FILE_BENCH_HPP_
#define _READ_FILE_BENCH_HPP_
#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>

template<typename R>
void runner(std::string s, R r)
{
  r.template operator()<false, false>(s);
  r.template operator()<false, true >(s + " EHP");
  r.template operator()<true , false>(s + " PAR");
  r.template operator()<true , true >(s + " PAR EHP");
}

void check_el_file_and_benchmark(pa c, std::string ELFile)
{
    std::string cmd = "head -n 1 " + ELFile + " && tail -n +2 " + ELFile + " | sort -t' ' -k1,1n -k2,2n";
    std::ifstream graphFile(ELFile.c_str());
    FILE* fpG = popen(cmd.c_str(), "r");
    auto fscanf_help = [fpG] (uint64_t& fst, uint64_t& snd) {
      return fscanf(fpG, "%" PRIu64 " %" PRIu64 "\n", &fst, &snd);
    };
    if (!graphFile.is_open())
    {
      std::cerr << "UNABLE TO open graphFile: " << ELFile
                << "\terrno: " << errno
                << "\terrstr: " << std::strerror(errno)
                << std::endl;

      exit(-2);
    }

    std::vector<std::pair<uint64_t,uint64_t>> s_edges;
    uint64_t s_num_nodes;
    uint64_t src;
    uint64_t dest;

    int num = fscanf_help(s_num_nodes, src);
    REQUIRE( num == 2 );
    while(2 == fscanf_help(src, dest))
    {
      s_edges.emplace_back(src, dest);
    }
    pclose(fpG);

    uint64_t numNodes;

    std::vector<std::pair<uint64_t,uint64_t>> edges;

    graphFile >> numNodes;
    //Gets rid of useless character
    graphFile >> src;

    while(graphFile >> src && graphFile >> dest)
    {
      edges.emplace_back(src, dest);
    }

    auto f = [c, &edges, numNodes, &s_edges, s_num_nodes]<bool par, bool ehp>(std::string s)
    {
      run_test_and_benchmark<Graph<par, ehp>*>(s, c, 7,
        []{ return new Graph<par, ehp>();},
        [&edges,numNodes](Graph<par, ehp>* g) { g->ingestEdgeList(numNodes, edges);},
        [&s_edges, s_num_nodes](Graph<par, ehp>* g)
        {

          REQUIRE( g->get_num_nodes() == s_num_nodes );

          uint64_t count = 0;

          for(const auto& [s,d] : s_edges)
          {
            REQUIRE( count < g->get_edge_end() );
            REQUIRE( g->get_edge(count) != nullptr );
            REQUIRE( g->get_edge(count)->get_dest() == d );
            REQUIRE( g->get_edge(count)->is_tomb() == false );
            REQUIRE( s < g->get_num_nodes() );
            REQUIRE( g->get_node(s) != nullptr );
            REQUIRE( g->get_node(s)->is_tomb() == false );
            REQUIRE( g->get_node(s)->start.get_value() <= count );
            REQUIRE( g->get_node(s)->stop > count );
            count++;
          }

          REQUIRE( count == g->get_edge_end() );
        },
        [](Graph<par, ehp>* g) { delete g; });
    };

    runner("Sequentially Ingest Graph from " + ELFile, f);

}
#endif
