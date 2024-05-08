# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

set(sources
        bc_mr.cpp
)

add_executable(bc_mr)
target_sources(bc_mr PRIVATE ${sources})
target_include_directories(bc_mr PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY} ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(bc_mr PRIVATE Galois::cusp Galois::dist_async Galois::gluon Galois::wmd)
