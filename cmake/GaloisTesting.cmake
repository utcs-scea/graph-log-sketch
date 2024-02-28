# SPDX-License-Identifier: MIT
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

### Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved. ###

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

# # Adds a source file as a GoogleTest based test that uses the emulator driver
# function(pando_add_driver_test TARGET SOURCEFILE)
#   if (NOT DEFINED ${PANDO_TEST_DISCOVERY_TIMEOUT})
#     set(DRIVER_DISCOVERY_TIMEOUT 15) # use 15s to avoid GASNet smp occasional init delays
#   else ()
#     set(DRIVER_DISCOVERY_TIMEOUT ${PANDO_TEST_DISCOVERY_TIMEOUT})
#   endif ()

#   if (PANDO_RT_BACKEND STREQUAL "DRVX")
#     set(HTHREADS "-p 4")
#     set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/scripts/run-drv.sh)
#   else ()
#     set(HTHREADS "")
#     if (${GASNet_CONDUIT} STREQUAL "smp")
#       set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/pando-rt/scripts/preprun.sh)
#     elseif (${GASNet_CONDUIT} STREQUAL "mpi")
#       set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/pando-rt/scripts/preprun_mpi.sh)
#     else ()
#       message(FATAL_ERROR "No runner script for GASNet conduit ${GASNet_CONDUIT}")
#     endif ()
#   endif()
#   set(NUM_PXNS 2)
#   set(NUM_CORES 4)

#   pando_add_executable(${TARGET} ${PROJECT_SOURCE_DIR}/test/test_driver.cpp ${SOURCEFILE})

#   if (PANDO_RT_BACKEND STREQUAL "DRVX")
#     target_link_libraries(${TARGET} PRIVATE
#       "$<LINK_LIBRARY:WHOLE_ARCHIVE,GTest::gtest>")
#     set_target_properties(gtest PROPERTIES POSITION_INDEPENDENT_CODE ON)

#     # create a dummy executable as a different target but with the same name for ctest to discover the right test programs
#     add_executable(${TARGET}-drvx ${PROJECT_SOURCE_DIR}/test/test_dummy.cpp)
#     set_target_properties(${TARGET}-drvx PROPERTIES OUTPUT_NAME ${TARGET})

#     # Robust method to bail on error: create regex variable to track failed tests via stdout/stderr rather than rely on return code of drvx-sst
#     set(FAIL_REGEX "FAIL_REGULAR_EXPRESSION;Failed;FAIL_REGULAR_EXPRESSION;ERROR;FAIL_REGULAR_EXPRESSION;FAILED;")
#   else()
#     target_link_libraries(${TARGET}
#       PRIVATE
#         GTest::gtest
#     )
#   endif()

#   pando_compiler_options(${TARGET})
#   pando_compiler_warnings(${TARGET})

#   # use CROSSCOMPILING_EMULATOR to point to the driver script
#   # it cannot be added to set_target_properties
#   set_property(TARGET ${TARGET}
#     PROPERTY CROSSCOMPILING_EMULATOR '${DRIVER_SCRIPT} ${HTHREADS} -n ${NUM_PXNS} -c ${NUM_CORES}'
#   )

#   if (${PANDO_TEST_DISCOVERY})
#     gtest_discover_tests(${TARGET}
#       DISCOVERY_TIMEOUT ${PANDO_TEST_DISCOVERY_TIMEOUT}
#     )
#   endif ()
# endfunction()

# function(pando_add_bin_test TARGET ARGS INPUTFILE OKFILE)
#   if (NOT PANDO_RT_BACKEND STREQUAL "DRVX")
#     if (${GASNet_CONDUIT} STREQUAL "smp")
#       set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/pando-rt/scripts/preprun.sh)
#     elseif (${GASNet_CONDUIT} STREQUAL "mpi")
#       set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/pando-rt/scripts/preprun_mpi.sh)
#     else ()
#       message(FATAL_ERROR "No runner script for GASNet conduit ${GASNet_CONDUIT}")
#     endif ()

#     set(NUM_PXNS 2)
#     set(NUM_CORES 4)

#     add_test(NAME ${TARGET}-${INPUTFILE}-${OKFILE}
#       COMMAND bash -c "diff -Z <(${DRIVER_SCRIPT} -n ${NUM_PXNS} -c ${NUM_CORES} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET} ${ARGS} ${INPUTFILE}) ${OKFILE}")
#   else()

#     set(DRIVER_SCRIPT ${PROJECT_SOURCE_DIR}/scripts/run-drv.sh)

#     set(NUM_PXNS 2)
#     set(NUM_CORES 4)
#     set(NUM_HTHREADS 8)

#     get_filename_component(FNAME ${TARGET} NAME)

#     add_test(NAME ${TARGET}-${INPUTFILE}-${OKFILE}
#       COMMAND bash -c "diff -Z <(LAUNCH_DIR=${CMAKE_SOURCE_DIR} ${DRIVER_SCRIPT} -p ${NUM_HTHREADS} -n ${NUM_PXNS} -c ${NUM_CORES} \
#       ${CMAKE_CURRENT_BINARY_DIR}/lib${FNAME}.so ${ARGS} ${INPUTFILE} \
#       | grep -v 'memBackendConverter:' |grep -v 'PANDOHammerDrvX:' |grep -v 'Simulation is complete, simulated time:' | tail -n +3) \
#       ${OKFILE}")

#   endif()
# endfunction()
