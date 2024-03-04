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

# helper functions
include(cmake/GaloisCompilerOptions.cmake)

if(CMAKE_CXX_FLAGS)
    message(STATUS "Provided CXX Flags: " ${CMAKE_CXX_FLAGS})
endif()

set(GALOIS_ENABLE_DIST ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# MPI Dependency
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

# Boost Dependency
find_package(Boost 1.74 COMPONENTS program_options REQUIRED)

set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(galois EXCLUDE_FROM_ALL)
set(BUILD_TESTING "${BUILD_TESTING_SAVED}")

add_subdirectory(microbench)

# tests
option(BUILD_TESTING "Build tests." ON)
if (BUILD_TESTING)
    include(cmake/GaloisTesting.cmake)
    enable_testing()
    add_subdirectory(test)
endif ()
