// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <numeric>
#include <string>
#include <vector>

#include <fstream>

void sampleToFile(const std::string& filename,
                  const std::vector<std::size_t>& samples) {
  std::ofstream ofs(filename);
  for (auto&& x : samples) {
    ofs << x << std::endl;
  }
  ofs.close();
}

std::vector<std::size_t> sampleGen(std::size_t start, std::size_t stop,
                                   std::size_t k, std::string filename = "") {
  std::vector<std::size_t> population(stop - start);
  std::iota(population.begin(), population.end(), start);
  auto rd  = std::random_device{};
  auto rng = std::mt19937{rd()};
  std::shuffle(population.begin(), population.end(), rng);
  std::vector<std::size_t> samples(
      population.begin(),
      std::next(population.begin(), k < (stop - start) ? k : (stop - start)));
  if (filename != "") {
    sampleToFile(filename, samples);
  }
  return samples;
}

std::vector<std::size_t> sampleFromFile(std::string filename) {
  std::vector<std::size_t> samples;
  std::ifstream ifs(filename);
  assert(ifs.is_open());
  std::size_t x;
  while (ifs >> x) {
    samples.push_back(x);
  }
  ifs.close();
  return samples;
}
