// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>

#include "galois/DistGalois.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  galois::DistMemSys G;
  int result = RUN_ALL_TESTS();
  return result;
}
