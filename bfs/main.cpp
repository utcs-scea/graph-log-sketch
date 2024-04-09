template <bool async>
struct BFS {
  Graph* graph;
  using DGTerminatorDetector =
      typename std::conditional<async, galois::DGTerminator<unsigned int>,
                                galois::DGAccumulator<unsigned int>>::type;

  DGTerminatorDetector& active_vertices;

  BFS(Graph* _graph, DGTerminatorDetector& _dga)
      : graph(_graph), active_vertices(_dga) {}

  void static go(Graph& _graph) {
    unsigned _num_iterations = 0;
    DGTerminatorDetector dga;

    const auto& nodesWithEdges = _graph.allNodesWithEdgesRange();
    do {
      syncSubstrate->set_num_round(_num_iterations);
      dga.reset();
    galois::do_all(
        galois::iterate(nodesWithEdges), BFS(&_graph, dga),
        galois::no_stats(), galois::steal(),
        galois::loopname(syncSubstrate->get_run_identifier("BFS").c_str()));
    syncSubstrate->sync<writeSource, readDestination, Reduce_min_dist_current,
                          Bitset_dist_current, async>("BFS");

      galois::runtime::reportStat_Tsum(
          REGION_NAME, syncSubstrate->get_run_identifier("NumWorkItems"),
          (unsigned long)dga.read_local());
      ++_num_iterations;
    } while ((async || (_num_iterations < maxIterations)) &&
             dga.reduce(syncSubstrate->get_run_identifier()));

    if (galois::runtime::getSystemNetworkInterface().ID == 0) {
      galois::runtime::reportStat_Single(
          REGION_NAME,
          "NumIterations_" + std::to_string(syncSubstrate->get_run_num()),
          (unsigned long)_num_iterations);
    }
  }