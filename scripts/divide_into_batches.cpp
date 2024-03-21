// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <stdint.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <limits>
#include <string>
#include <algorithm>
#include <random>
#include <optional>

#include <boost/program_options.hpp>

int main(int argc, char const* argv[]) {
  // NOLINTNEXTLINE
  using namespace std;

  namespace po = boost::program_options;
  po::options_description desc("Divide edge list into batches");
  desc.add_options()                                            //
      ("help,h", "Print help messages")                         //
      ("input", po::value<string>(), "Input file path")         //
      ("output", po::value<string>(), "Output file path")       //
      ("num_batches", po::value<size_t>(), "Number of batches") //
      ("randomize", po::bool_switch()->default_value(false),
       "Shuffle edges before batching")                                      //
      ("rseed", po::value<unsigned int>()->default_value(0), "random seed"); //

  po::variables_map vm;
  try {
    // Parse command line arguments
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (po::error& e) {
    cout << e.what() << endl;
    cout << desc << endl;
  }

  // Check for help option
  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  ifstream fin(vm["input"].as<string>());
  if (!fin.is_open()) {
    cerr << "Error opening input file" << endl;
    return 1;
  }

  ofstream fout(vm["output"].as<string>());
  if (!fout.is_open()) {
    cerr << "Error opening output file" << endl;
    return 2;
  }

  vector<pair<uint64_t, uint64_t>> edges;

  while (fin.peek() == '#')
    fin.ignore(numeric_limits<streamsize>::max(), '\n');

  uint64_t src, dst;
  while (fin >> src >> dst)
    edges.emplace_back(src, dst);

  size_t num_edges = edges.size();
  cout << "number of edges: " << num_edges << endl;

  uint64_t max_vertex_id = 0;

  auto max_pair = std::max_element(
      edges.begin(), edges.end(), [](const auto& lhs, const auto& rhs) {
        return max(lhs.first, lhs.second) < max(rhs.first, rhs.second);
      });

  cout << "maximum vertex id: " << max(max_pair->first, max_pair->second)
       << endl;

  srand(vm["rseed"].as<unsigned int>());
  if (vm["randomize"].as<bool>())
    random_shuffle(edges.begin(), edges.end());

  size_t num_batches = vm["num_batches"].as<size_t>();
  if (num_batches == 0) {
    cerr << "num_batches must be greater than zero" << endl;
    return 3;
  }

  size_t edges_per_batch = (num_edges + (num_batches - 1)) / num_batches;

  for (size_t batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
    auto begin = edges.begin() + batch_idx * edges_per_batch;
    auto end =
        edges.begin() + std::min(num_edges, (batch_idx + 1) * edges_per_batch);
    std::sort(begin, end);
    optional<uint64_t> prev_src;

    for (auto it = begin; it != end; ++it) {
      uint64_t next_src = it->first;
      uint64_t next_dst = it->second;
      if (prev_src != next_src) {
        if (prev_src.has_value())
          fout << endl;

        fout << next_src << ' ' << next_dst;
        prev_src = next_src;
      } else {
        fout << ' ' << next_dst;
      }
    }
    fout << endl << endl;
  }

  return 0;
}
