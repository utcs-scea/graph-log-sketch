# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

add_executable(wf4_import_test ${graph-log-sketch_SOURCE_DIR}/test/test_driver.cpp test_import.cpp)
target_link_libraries(wf4_import_test GTest::gtest_main wf4-galois::wf4-galois)
set_property(TARGET wf4_import_test PROPERTY CROSSCOMPILING_EMULATOR 'mpirun -np 2')

add_executable(wf4_influencer_test test_influencer.cpp)
target_link_libraries(wf4_influencer_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(wf4_import_test)
gtest_discover_tests(wf4_influencer_test)
