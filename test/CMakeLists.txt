# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

galois_add_test(bfs-test Galois::shmem test-algo-bfs.cpp)
target_include_directories(bfs-test PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)

galois_add_test(tc-test Galois::shmem test-algo-tc.cpp)
target_include_directories(tc-test PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)

galois_add_test(bc-test Galois::shmem test-algo-bc.cpp)
target_include_directories(bc-test PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)

add_subdirectory(wf4)
