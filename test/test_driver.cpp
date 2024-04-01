// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>

#include "galois/DistGalois.h"

namespace {

bool isDiscoveringTests(int argc, char** argv) {
  for (int64_t i = 0; i < argc; i++) {
    if (std::string(argv[i]) == "--gtest_list_tests") {
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  bool discoveringTests = isDiscoveringTests(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  galois::DistMemSys G;
  auto& net  = galois::runtime::getSystemNetworkInterface();
  int result = 0;

  if (net.ID == 0 || !discoveringTests) {
    result = RUN_ALL_TESTS();
  }
  return result;
}
