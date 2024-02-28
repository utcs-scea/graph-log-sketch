include(${graph-log-sketch_SOURCE_DIR}/cmake/GaloisCompilerOptions.cmake)

# edit scalability
galois_add_executable(edit-scalability edit_scalability.cpp)
target_link_libraries(edit-scalability PRIVATE Galois::shmem ${llvm_libs})
