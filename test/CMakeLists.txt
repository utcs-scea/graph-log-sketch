galois_add_test(bfs-test test-algo-bfs.cpp)
target_link_libraries(bfs-test PRIVATE Galois::shmem)
target_include_directories(bfs-test PRIVATE ${graph-log-sketch_SOURCE_DIR}/include)
