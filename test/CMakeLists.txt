# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

galois_add_test(bfs-test test-algo-bfs.cpp)
target_link_libraries(bfs-test PRIVATE Galois::shmem)
target_include_directories(bfs-test PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)
