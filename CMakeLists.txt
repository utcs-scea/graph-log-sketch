cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
project(graph-log-sketch)
include_directories(include)
include_directories(wf1/include)
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.3.2 # or a later release
)

FetchContent_MakeAvailable(Catch2)

execute_process(COMMAND mkdir -p pytorch/libtorch_build/
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

execute_process(COMMAND python ../tools/build_libtorch.py
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pytorch/libtorch_build)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pytorch/torch")
find_package(Torch REQUIRED)
#message(TORCH_CXX_FLAGS="${TORCH_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

add_subdirectory(galois EXCLUDE_FROM_ALL)
add_subdirectory(CombBLAS EXCLUDE_FROM_ALL)

add_subdirectory(wf1)

add_executable(small-tests test/small_tests.cpp)
add_executable(file-tests test/file_tests.cpp)
add_executable(dijkstra-tests test/dijkstra.cpp)
add_executable(bfs-tests test/bfs.cpp)
add_executable(galois-tests test/galois.cpp)
add_executable(galois-bfs-tests test/galois_bfs.cpp)
add_executable(galois-gen-bfs-tests test/galois_gen_bfs.cpp)
add_executable(galois-jaccard-tests test/jaccard.cpp)
add_executable(jaccard microbench/jaccard.cpp)
add_executable(edit_scalability microbench/edit_scalability.cpp)
add_executable(parallel-small-tests test/p_small_tests.cpp)
add_executable(ego-tests test/ego_graph.cpp)
add_executable(do-workflow1 wf1/main.cpp)
add_executable(gen-rand-bfs microbench/gen_rand_bfs.cpp)
add_executable(gen-rand-tricount microbench/gen_rand_tricount.cpp)

target_link_libraries(do-workflow1 workflow1 Galois::shmem)
target_link_libraries(small-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(file-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(dijkstra-tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(bfs-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(galois-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(galois-bfs-tests PRIVATE Catch2::Catch2WithMain Galois::shmem lonestar)
target_link_libraries(galois-gen-bfs-tests PRIVATE Catch2::Catch2WithMain Galois::shmem lonestar)
target_link_libraries(galois-jaccard-tests PRIVATE Catch2::Catch2WithMain Galois::shmem lonestar)
target_link_libraries(parallel-small-tests PRIVATE Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(ego-tests PRIVATE "${TORCH_LIBRARIES}" Catch2::Catch2WithMain Galois::shmem)
target_link_libraries(jaccard PRIVATE Galois::shmem lonestar)
target_link_libraries(edit_scalability PRIVATE Galois::shmem lonestar)
target_link_libraries(gen-rand-bfs PRIVATE CombBLAS Galois::shmem lonestar)
target_link_libraries(gen-rand-tricount PRIVATE "${MPI_CXX_LIBRARIES}" Galois::shmem lonestar)

include(CTest)
include(Catch)
catch_discover_tests(small-tests)
catch_discover_tests(ego-tests)
catch_discover_tests(file-tests)
catch_discover_tests(dijkstra-tests)
catch_discover_tests(bfs-tests)
catch_discover_tests(galois-tests)
catch_discover_tests(galois-bfs-tests)
catch_discover_tests(galois-jaccard-tests)
catch_discover_tests(galois-gen-bfs-tests)
catch_discover_tests(parallel-small-tests)
