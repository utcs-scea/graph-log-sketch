# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

set(sources
        bfs.cpp
)

add_executable(bfs)
target_sources(bfs PRIVATE ${sources})
target_include_directories(bfs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY} ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(bfs PRIVATE Galois::cusp Galois::dist_async Galois::gluon Galois::wmd Boost::program_options Scea::lib)
