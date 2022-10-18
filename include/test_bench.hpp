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
  r.template operator()<false, false>(s + " EHP");
  r.template operator()<false, false>(s + " PAR");
  r.template operator()<false, false>(s + " PAR EHP");
}

void check_el_file_and_benchmark(pa c, std::string ELFile)
{
    std::string cmd = "head -n 1 " + ELFile + " && tail -n +2 " + ELFile + " | sort -t' ' -k1,1n -k2,2n";
    std::ifstream graphFile(ELFile.c_str());
    if (!graphFile.is_open())
    {
      std::cerr << "UNABLE TO open graphFile: " << ELFile
                << "\terrno: " << errno
                << "\terrstr: " << std::strerror(errno)
                << std::endl;

      exit(-2);
    }
    uint64_t numNodes;
    uint64_t src;
    uint64_t dest;
    std::vector<uint64_t> srcs;
    std::vector<uint64_t> dests;

    graphFile >> numNodes;
    //Gets rid of useless character
    graphFile >> src;

    while(graphFile >> src && graphFile >> dest)
    {
      srcs.push_back(src);
      dests.push_back(dest);
    }

    auto f = [c, &srcs, &dests, numNodes, cmd]<bool par, bool ehp>(std::string s)
    {
      run_test_and_benchmark<Graph<par, ehp>*>(s, c, 7,
        []{ return new Graph<par, ehp>();},
        [&srcs,&dests,numNodes](Graph<par, ehp>* g) { g->ingestEdgeList(numNodes, srcs, dests);},
        [cmd](Graph<par, ehp>* g)
        {
          FILE* fpG = popen(cmd.c_str(), "r");
          auto fscanf_help = [fpG] (uint64_t& fst, uint64_t& snd) {
            return fscanf(fpG, "%" PRIu64 " %" PRIu64 "\n", &fst, &snd);
          };

          uint64_t num_nodes;
          uint64_t useless;
          int num = fscanf_help(num_nodes, useless);

          REQUIRE( num == 2 );

          REQUIRE( g->get_num_nodes() == num_nodes );

          uint64_t count = 0;
          uint64_t src;
          uint64_t dest;

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
        },
        [](Graph<par, ehp>* g) { delete g; });
    };

    runner("Sequentially Ingest Graph from " + ELFile, f);

}
#endif
