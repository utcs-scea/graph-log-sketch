# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(
    graph-log-sketch
    VERSION       0.1.0
    DESCRIPTION   "Monorepo for Galois based workflows using Log Structured CSR"
    HOMEPAGE_URL  "https://github.com/utcs-scea/graph-log-sketch"
    LANGUAGES     C CXX
)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_FLAGS_COVERAGE "-g -O0 --coverage -fprofile-abs-path")
    set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")
    set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "--coverage")
    set(CMAKE_MODULE_LINKER_FLAGS_COVERAGE "--coverage")
endif()

# helper functions
include(cmake/GaloisCompilerOptions.cmake)

if(PROJECT_IS_TOP_LEVEL)
    if(CMAKE_CXX_FLAGS)
        message(STATUS "Provided CXX Flags: " ${CMAKE_CXX_FLAGS})
    endif()

    set(CMAKE_OPTIMIZE_DEPENDENCIES true)

    # Setup CCache
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        message(STATUS "CCache found at: " ${CCACHE_EXECUTABLE})
        set(ccacheEnv
            CCACHE_SLOPPINESS=pch_defines,time_macros
        )
        # NOTE: Ccache 4.2+ required for reliable CUDA support
        foreach(lang IN ITEMS C CXX OBJC OBJCXX CUDA)
            set(CMAKE_${lang}_COMPILER_LAUNCHER
                ${CMAKE_COMMAND} -E env ${ccacheEnv} ${CCACHE_EXECUTABLE}
            )
        endforeach()
    endif()
endif()

set(GALOIS_ENABLE_DIST ON CACHE BOOL "" FORCE)
set(GALOIS_ENABLE_WMD ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# parallel-hashmap dependency
include_directories(${CMAKE_CURRENT_LIST_DIR}/galois/external/parallel-hashmap)

# MPI Dependency
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

# Boost Dependency
find_package(Boost 1.72 COMPONENTS program_options REQUIRED)

set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(galois EXCLUDE_FROM_ALL)
set(BUILD_TESTING "${BUILD_TESTING_SAVED}")

set(PHMAP_DIRECTORY ${Galois_SOURCE_DIR}/external/parallel-hashmap)
set(PCG_DIRECTORY ${Galois_SOURCE_DIR}/external/pcg-cpp/include)

add_subdirectory(microbench)
add_subdirectory(scripts)
add_subdirectory(bfs)
add_subdirectory(cc)
add_subdirectory(pagerank)

# tests
option(BUILD_TESTING "Build tests." ON)
if (BUILD_TESTING)
    include(cmake/GaloisTesting.cmake)
    enable_testing()
    add_subdirectory(test)
endif ()
