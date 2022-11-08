cmake_minimum_required(VERSION 3.11)
project(graph-log-sketch)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
include_directories(include)
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.1.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_subdirectory(galois EXCLUDE_FROM_ALL)

add_executable(small-tests test/small_tests.cpp)
add_executable(file-tests test/file_tests.cpp)
add_executable(dijkstra-tests test/dijkstra.cpp)
add_executable(galois-tests test/galois.cpp)
target_link_libraries(small-tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(file-tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(dijkstra-tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(galois-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
include(CTest)
include(Catch)
catch_discover_tests(small-tests)
catch_discover_tests(file-tests)
catch_discover_tests(dijkstra-tests)
catch_discover_tests(galois-tests)
