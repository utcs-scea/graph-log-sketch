# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

include(${graph-log-sketch_SOURCE_DIR}/cmake/GaloisCompilerOptions.cmake)

# edit scalability
galois_add_executable(edit-scalability edit_scalability.cpp)
target_link_libraries(edit-scalability PRIVATE Galois::shmem Boost::program_options)
target_include_directories(edit-scalability PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)
