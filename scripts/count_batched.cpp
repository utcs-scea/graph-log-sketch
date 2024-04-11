// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/program_options.hpp>

int main(int ac, char const* av[]) {
  bool quiet         = false;
  bool show_vertices = false;
  bool show_edges    = false;

  namespace po = boost::program_options;
  po::options_description generic("Generic Options");
  generic.add_options()("help,h", "Print help messages")                      //
      ("quiet,q", po::bool_switch(&quiet), "Print only the requested counts") //
      ("vertices,v", po::bool_switch(&show_vertices),
       "Print one larger than the maximum vertex ID")                         //
      ("edges,e", po::bool_switch(&show_edges), "Print the number of edges"); //

  po::options_description hidden("Hidden options");
  hidden.add_options()("input-file", po::value<std::string>(), "Input file");

  po::options_description cmdline_options;
  cmdline_options.add(generic).add(hidden);

  po::positional_options_description p;
  p.add("input-file", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(ac, av)
                  .options(cmdline_options)
                  .positional(p)
                  .run(),
              vm);
    po::notify(vm);
  } catch (po::error& e) {
    std::cout << e.what() << std::endl;
    std::cout << generic << std::endl;
  }

  if (vm.count("help")) {
    std::cout << generic << std::endl;
    return 1;
  }

  if (!vm.count("input-file")) {
    std::cerr << "No input file specified." << std::endl;
    return 1;
  } else if (vm.count("input-file") > 1) {
    std::cerr << "Only one input file should be specified." << std::endl;
    return 1;
  }

  if (!show_vertices && !show_edges) {
    show_vertices = show_edges = true;
  }

  std::string input_file_path = vm["input-file"].as<std::string>();
  std::istream* in            = &std::cin;
  std::ifstream input_file;
  if (input_file_path != "-") {
    input_file.open(input_file_path);
    in = &input_file;

    if (!input_file.is_open()) {
      std::cerr << "Could not open input file: " << input_file_path
                << std::endl;
      return 1;
    }
  }

  uint64_t max_vertex_id = 0, num_edges = 0;

  // Read the input file and compute the requested counts

  while (!in->eof()) { // for each batch
    /*
     * each line is a parallel insertion, of the form:
     * ```
     * src dst1 dst2 dst3 ...
     * ```
     */

    std::string batch_raw;
    while (std::getline(*in, batch_raw)) {
      if (batch_raw.length() == 0)
        break;

      std::istringstream batch(batch_raw);

      uint64_t src;
      batch >> src;
      max_vertex_id = std::max(max_vertex_id, src);

      while (!batch.eof()) {
        uint64_t tmp;
        batch >> tmp;
        max_vertex_id = std::max(max_vertex_id, tmp);
        ++num_edges;
      }
    }
  }

  if (show_vertices) {
    if (!quiet)
      std::cout << "max_vertex_id + 1: ";
    std::cout << max_vertex_id + 1 << std::endl;
  }

  if (show_edges) {
    if (!quiet)
      std::cout << "num_edges: ";
    std::cout << num_edges << std::endl;
  }

  return 0;
}
