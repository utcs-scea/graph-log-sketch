cmake_minimum_required(VERSION 3.11)
Include(FetchContent)

project(graph-log-sketch)

add_subdirectory(galois EXCLUDE_FROM_ALL)
set(GALOIS_ENABLE_DIST ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# LLVM dependency for CLI argument parsing
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support)

# add include directories
include_directories(include ${LLVM_INCLUDE_DIRS})

# edit scalability
add_executable(edit-scalability microbench/edit_scalability.cpp)
target_link_libraries(edit-scalability PRIVATE Galois::shmem ${llvm_libs})

# unit tests
find_package(Catch2 3)
IF( Catch2_FOUND )
    file(GLOB_RECURSE test_SOURCES "test/*.cpp")

    add_executable(tests ${test_SOURCES})
    target_link_libraries(tests PRIVATE Galois::shmem Catch2::Catch2WithMain)

    include(CTest)
    include(Catch)
    catch_discover_tests(tests)
ENDIF()
