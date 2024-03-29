# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

enable_testing()

add_executable(influencer_test influencer_test.cpp)
target_link_libraries(influencer_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(influencer_test)
