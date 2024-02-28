galois_add_test(bfs-test test-algo-bfs.cpp)
target_link_libraries(bfs-test PRIVATE Galois::shmem)
