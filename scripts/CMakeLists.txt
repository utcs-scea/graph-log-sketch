# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

# scripts
add_executable(divide-into-batches divide_into_batches.cpp)
target_link_libraries(divide-into-batches PRIVATE Boost::program_options)

add_executable(count-batched count_batched.cpp)
target_link_libraries(count-batched PRIVATE Boost::program_options)

add_executable(shufbel shufbel.cpp)
target_link_libraries(shufbel PRIVATE Boost::program_options)
