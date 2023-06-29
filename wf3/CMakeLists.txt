set(sources
        src/fasta.cpp
        src/pakman.cpp
        src/util.cpp
)

add_executable(pakman)
target_sources(pakman PRIVATE ${sources})
target_include_directories(pakman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pakman PRIVATE Galois::shmem Galois::dist_async lonestar)
