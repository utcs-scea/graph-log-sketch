# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

set(sources
        src/import.cpp
        src/influencer.cpp
        src/main.cpp
        src/quiesce.cpp
)

add_executable(wf4)
target_sources(wf4 PRIVATE ${sources})
target_include_directories(wf4 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include ${PHMAP_DIRECTORY} ${PCG_DIRECTORY}/include ${graph-log-sketch_SOURCE_DIR}/include)
target_link_libraries(wf4 PRIVATE Galois::cusp Galois::dist_async Galois::gluon LLVMSupport)
