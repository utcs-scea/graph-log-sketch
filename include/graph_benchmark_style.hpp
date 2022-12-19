#include <benchmark.hpp>

template<typename Graph, typename Setup, typename Algo>
void run_algo_static(std::string& statsfn, std::string msg, pa c, int cpu,
    uint64_t num_nodes, uint64_t num_edges,
    std::vector<uint64_t>* edge_list,
    Setup setup, Algo algo)
{
  Graph graph = Graph(num_nodes, num_edges, [edge_list](uint32_t n){return edge_list[n].size();},
    [edge_list](uint64_t n, uint64_t e) {return edge_list[n][e];}, [](uint64_t n, uint64_t e){ return 0; });

  run_benchmark<uint64_t>(statsfn, msg, c, cpu,
    [&graph,setup]{setup(graph); return 0;},
    [&graph,algo](uint64_t v){algo(graph);},
    [](uint64_t v){});
}

template<typename Graph, typename Evo, typename Setup, typename Algo>
void run_algo_evolve(std::string& statsfn, std::string msg, pa c, int cpu,
    uint8_t chunk_num, uint64_t num_nodes, uint64_t num_edges,
    const std::vector<std::pair<uint64_t,uint64_t>>& edge_list,
    Evo evo, Setup setup, Algo algo)
{
  auto g = [num_nodes]() {
    auto graph = new Graph(num_nodes, 0, [](uint64_t n) {return 0;},
      [](uint64_t n, uint64_t e) {return 0;}, [](uint64_t n, uint64_t e){ return 0;});
    return graph;
  };

  auto f = [edge_list,g,num_edges,msg,c,algo,evo,cpu,chunk_num,setup,statsfn](uint8_t i)
  {
    std::string blah = " " + std::to_string(i) + "/" + std::to_string(chunk_num);
    //Benchmark the Edits
    run_benchmark<Graph*>(statsfn, msg + " Edit" + blah,
      c, cpu,
      [g,evo,num_edges,i,edge_list,chunk_num]
      {
        Graph* graph = g();
        evo(graph, 0, num_edges/chunk_num * (i - 1), num_edges/chunk_num, edge_list);
        return graph;
      },
      [edge_list,num_edges,i,evo,chunk_num](Graph* graph){
        evo(graph, num_edges/chunk_num * (i-1), num_edges/chunk_num, num_edges/chunk_num, edge_list);
      },[](Graph* graph){delete graph;});

    //Generate the graph fresh for the Algorithm
    Graph* graph = g();
    evo(graph, 0, num_edges/chunk_num * i, num_edges/chunk_num, edge_list);

    run_benchmark<uint64_t>(statsfn, msg + " Algo" + blah,
      c, cpu,
      [graph,setup]{setup(*graph); return 0;},
      [graph,algo](uint64_t v){
        algo(*graph);
      }, [](uint64_t v){});

    delete graph;
  };

  for(uint8_t i = 1; i <= chunk_num; i++) f(i);
}

template<typename Graph>
void regen_graph_sorted(Graph* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
{
  auto edge_list = new std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>[graph->size()];
  for(uint64_t i = 0; i < upper && i < edges.size(); i++)
    edge_list[edges[i].first].push(edges[i].second);

  Graph* graph2 = new Graph(graph->size(), upper,
      [edge_list](uint64_t n){return edge_list[n].size();},
      [edge_list](uint64_t n, uint64_t e) {auto elem = edge_list[n].top(); edge_list[n].pop(); return elem;},
      [](uint32_t n, uint64_t e){return 0;});

  swap(*graph, *graph2);
  delete graph2;
  delete[] edge_list;
}

template<typename Graph>
void regen_graph_asis(Graph* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
{
  std::vector<uint64_t>* edge_list = new std::vector<uint64_t>[graph->size()];

  for(uint64_t i = 0; i < upper && i < edges.size(); i++)
    edge_list[edges[i].first].emplace_back(edges[i].second);

  Graph* graph2 = new Graph(graph->size(), upper,
      [edge_list](uint64_t n){return edge_list[n].size();},
      [edge_list](uint64_t n, uint64_t e) {return edge_list[n][e];},
      [](uint64_t n, uint64_t e){ return 0; });

  swap(*graph, *graph2);
  delete graph2;
  delete[] edge_list;
}

template<typename Graph>
void add_edges_per_node(Graph* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
{
  for(uint64_t start = lower; start < upper; start += batch_size)
  {
    std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>* edge_list
      = new std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>[graph->size()];
    for(uint64_t i = start; i < start + batch_size && i < edges.size(); i++)
      edge_list[edges[i].first].push(edges[i].second);

    graph->addEdges(edge_list);

    delete[] edge_list;
  }
}

template<bool sort, typename Graph>
void add_edges_per_edge(Graph* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
{
  for(uint64_t j = lower; j < upper && j < edges.size(); j++)
    graph->addEdge(edges[j].first, edges[j].second);
  if(sort) graph->sortAllEdgesByDst();
}
