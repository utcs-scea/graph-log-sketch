cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(
    graph-log-sketch
    VERSION       0.1.0
    DESCRIPTION   "Monorepo for Galois based workflows using Log Structured CSR"
    HOMEPAGE_URL  "https://github.com/utcs-scea/graph-log-sketch"
    LANGUAGES     CXX
)

# helper functions
include(cmake/GaloisCompilerOptions.cmake)

if(CMAKE_CXX_FLAGS)
    message(STATUS "Provided CXX Flags: " ${CMAKE_CXX_FLAGS})
endif()

set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(galois EXCLUDE_FROM_ALL)
set(BUILD_TESTING "${BUILD_TESTING_SAVED}")

set(GALOIS_ENABLE_DIST ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# LLVM dependency for CLI argument parsing
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support)
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

# MPI Dependency
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

# add include directories
include_directories(include ${LLVM_INCLUDE_DIRS})

add_subdirectory(microbench)

# tests
option(BUILD_TESTING "Build tests." ON)
if (BUILD_TESTING)
    include(cmake/GaloisTesting.cmake)
    enable_testing()
    add_subdirectory(test)
endif ()
