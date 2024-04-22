// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <stdint.h>
#include <linux/mman.h>

#include <random>
#include <string>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

struct Edge {
  uint64_t src;
  uint64_t dst;

  friend inline void swap(Edge& lhs, Edge& rhs) {
    using std::swap;
    uint64_t src = lhs.src;
    uint64_t dst = lhs.dst;
    lhs.src      = rhs.src;
    lhs.dst      = rhs.dst;
    rhs.src      = src;
    rhs.dst      = dst;
  }
} __attribute__((packed));

static_assert(sizeof(Edge) == 16, "Edge must be 16 bytes.");

namespace bip = boost::interprocess;
namespace po  = boost::program_options;

int main(int argc, char const* argv[]) {
  po::options_description desc("Shuffle binary edge list in-place");
  desc.add_options()                                        //
      ("help,h", "Print help messages")                     //
      ("file", po::value<std::string>(), "Input file path") //
      ("rseed", po::value<unsigned int>()->default_value(0),
       "Fixed random seed");

  po::variables_map vm;
  try {
    // Parse command line arguments
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (po::error& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  if (vm.count("file") != 1) {
    std::cerr << "Must specify an input file with --file" << std::endl;
    return 1;
  }

  bip::file_mapping input_file_mapping(vm["file"].as<std::string>().c_str(),
                                       bip::read_write);
  bip::mapped_region mapped_rgn(input_file_mapping, bip::read_write, 0, 0,
                                nullptr, MAP_POPULATE);
  mapped_rgn.advise(bip::mapped_region::advice_willneed);

  // rng
  std::mt19937 rng(vm["rseed"].as<unsigned int>());
  std::shuffle(reinterpret_cast<Edge*>(mapped_rgn.get_address()),
               reinterpret_cast<Edge*>(
                   reinterpret_cast<char*>(mapped_rgn.get_address()) +
                   mapped_rgn.get_size()),
               rng);
  return 0;
}
