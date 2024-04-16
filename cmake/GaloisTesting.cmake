# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#
# dependencies
#

include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

include(${graph-log-sketch_SOURCE_DIR}/cmake/GaloisCompilerOptions.cmake)

#
# options
#

option(GALOIS_TEST_DISCOVERY "Enable test discovery for use with ctest." ON)
if (${GALOIS_TEST_DISCOVERY})
  set(GALOIS_TEST_DISCOVERY_TIMEOUT "" CACHE STRING "GoogleTest test discover timeout in seconds")
endif ()
set(GALOIS_TEST_DISCOVERY_TIMEOUT "120" CACHE STRING "Test timeout (secs)")

#
# functions
#

# Adds a source file as a GoogleTest based test
function(galois_add_test TARGET LIBRARY SOURCEFILE)
  galois_add_executable(${TARGET} ${SOURCEFILE})
  target_link_libraries(${TARGET}
    PRIVATE
      GTest::gtest_main
      ${LIBRARY}
  )
  galois_compiler_options(${TARGET})
  galois_compiler_warnings(${TARGET})

  if (${GALOIS_TEST_DISCOVERY})
    if (NOT DEFINED ${GALOIS_TEST_DISCOVERY_TIMEOUT})
      # use default test discovery timeout
      gtest_discover_tests(${TARGET})
    else ()
      gtest_discover_tests(${TARGET}
        DISCOVERY_TIMEOUT ${GALOIS_TEST_DISCOVERY_TIMEOUT}
      )
    endif ()
  endif ()
endfunction()

set(NUM_PROCS 2)

# Adds a source file as a GoogleTest based test using mpirun to launch it
function(galois_add_driver_test TARGET LIBRARY SOURCEFILE)
  galois_add_executable(${TARGET} ${graph-log-sketch_SOURCE_DIR}/test/test_driver.cpp ${SOURCEFILE})
  target_link_libraries(${TARGET}
    PRIVATE
      GTest::gtest
      ${LIBRARY}
  )
  galois_compiler_options(${TARGET})
  galois_compiler_warnings(${TARGET})
  set_property(TARGET ${TARGET} PROPERTY CROSSCOMPILING_EMULATOR 'mpirun -np ${NUM_PROCS}')

  if (${GALOIS_TEST_DISCOVERY})
    if (NOT DEFINED ${GALOIS_TEST_DISCOVERY_TIMEOUT})
      # use default test discovery timeout
      gtest_discover_tests(${TARGET})
    else ()
      gtest_discover_tests(${TARGET}
        DISCOVERY_TIMEOUT ${GALOIS_TEST_DISCOVERY_TIMEOUT}
      )
    endif ()
  endif ()
endfunction()
