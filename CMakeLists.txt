cmake_minimum_required(VERSION 3.11)
Include(FetchContent)

project(graph-log-sketch)

add_subdirectory(galois EXCLUDE_FROM_ALL)
set(GALOIS_ENABLE_DIST ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_package(Boost 1.82.0 COMPONENTS container algorithm)

include_directories(include ${BOOST_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(edit-scalability microbench/edit_scalability.cpp)

llvm_map_components_to_libnames(llvm_libs support)

target_link_libraries(edit-scalability PRIVATE Galois::shmem ${Boost_LIBRARY_DIRS} ${llvm_libs})