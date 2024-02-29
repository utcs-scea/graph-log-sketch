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

include(${PROJECT_SOURCE_DIR}/cmake/GaloisCompilerOptions.cmake)

#
# options
#

option(GALOIS_TEST_DISCOVERY "Enable test discovery for use with ctest." ON)
if (${GALOIS_TEST_DISCOVERY})
  set(GALOIS_TEST_DISCOVERY_TIMEOUT "" CACHE STRING "GoogleTest test discover timeout in seconds")
endif ()

#
# functions
#

# Adds a source file as a GoogleTest based test
function(galois_add_test TARGET SOURCEFILE)
  add_executable(${TARGET} ${SOURCEFILE})
  target_link_libraries(${TARGET}
    PRIVATE
      GTest::gtest_main
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
