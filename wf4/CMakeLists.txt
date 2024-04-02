# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

set(sources
        src/import.cpp
        src/influence_maximization.cpp
        src/quiesce.cpp
)

target_sources(wf4-galois PRIVATE ${sources})
target_include_directories(wf4-galois PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY} ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(wf4-galois PUBLIC Galois::cusp Galois::dist_async Galois::gluon Galois::wmd)

galois_add_executable(wf4 src/main.cpp)
target_link_libraries(wf4
    PUBLIC
        wf4-galois::wf4-galois
)
