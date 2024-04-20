# Create a library target
add_library(libdist INTERFACE)
add_library(Scea::lib ALIAS libdist)
add_dependencies(lib libdist)

# Set include directories for the library target
target_include_directories(libdist INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/scea)

# Optionally, you can add additional configurations or dependencies for the library target

install(TARGETS libdist
  EXPORT GLSTargets
  LIBRARY
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT shlib
  ARCHIVE
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT lib
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)