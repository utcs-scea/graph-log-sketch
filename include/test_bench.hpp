#ifndef _READ_FILE_BENCH_HPP_
#define _READ_FILE_BENCH_HPP_
#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <galois/graphs/MorphGraph.h>

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

    auto g = [c, &edges, numNodes, &s_edges, s_num_nodes, ELFile]()
    {
      using MGraph = galois::graphs::MorphGraph<void, void, true>;
      using GNode = MGraph::GraphNode;
      struct RType
      {
        MGraph* g;
        vector<GNode>* nodes;
      };
      run_test_and_benchmark<RType>("Sequentially Ingest Graph from " + ELFile + "Galois", c, 7,
        []{ RType r { new MGraph(), new vector<GNode>()}; return r; },
        [&edges,numNodes](RType r)
        {
          r.nodes->resize(numNodes);
          for(uint64_t i = 0; i < numNodes; i++)
          {
            GNode a = r.g->createNode();
            r.g->addNode(a);
            r.nodes->at(i) = a;
          }
          for(const auto& [s,d] : edges)
          {
            r.g->addEdge(r.nodes->at(s), r.nodes->at(d));
          }
        },
        [&s_edges, s_num_nodes](RType r)
        {

          REQUIRE( r.g->size() == s_num_nodes );

          // Does not check sorting
          for(const auto& [s,d] : s_edges)
          {
            const GNode& src = r.nodes->at(s);
            const GNode& dst = r.nodes->at(d);
            bool edge_found = r.g->findEdge(src, dst) != r.g->edge_end(src);
            REQUIRE( edge_found == true );
          }
        },
        [](RType r) { delete r.g; delete r.nodes; });
    };

    g();
    runner("Sequentially Ingest Graph from " + ELFile, f);

}

#endif
