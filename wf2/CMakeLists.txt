# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

set(sources
        src/import.cpp
        src/pattern.cpp
)

add_executable(wf2-exact src/main.cpp)
target_sources(wf2-exact PRIVATE ${sources})
target_include_directories(wf2-exact PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY} ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(wf2-exact PRIVATE Galois::cusp Galois::dist_async Galois::gluon Galois::wmd)

add_executable(wf2-exact-es src/edit_scalability_main.cpp)
target_sources(wf2-exact-es PRIVATE ${sources})
target_include_directories(wf2-exact-es PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY} ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(wf2-exact-es PRIVATE Galois::cusp Galois::dist_async Galois::gluon Galois::wmd)
