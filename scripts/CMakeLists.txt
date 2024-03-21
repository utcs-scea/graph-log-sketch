# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

# scripts
add_executable(divide_into_batches divide_into_batches.cpp)
target_link_libraries(divide_into_batches PRIVATE Boost::program_options)
