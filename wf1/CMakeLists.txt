add_library(workflow1 STATIC readFiles.cpp csr.cpp)
target_link_libraries(workflow1 Galois::shmem)