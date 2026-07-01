include(ExternalProject)
include(GNUInstallDirs)

set(EXTRAE_PREFIX "${CMAKE_BINARY_DIR}/_deps/extrae-install"
  CACHE PATH "extrae install prefix")
set(EXTRAE_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/extrae-src"
  CACHE PATH "extrae source tree")
set(EXTRAE_LIBRARY "${EXTRAE_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libextrae.a"
  CACHE FILEPATH "extrae static library")
set(EXTRAE_INCLUDE_DIR "${EXTRAE_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/extrae"
  CACHE PATH "extrae include directory")

ExternalProject_Add(extrae_ep
  GIT_REPOSITORY https://github.com/extrae/extrae-src
  GIT_TAG        v1.0.0

  SOURCE_DIR  "${EXTRAE_SOURCE_DIR}"
  BINARY_DIR  "${CMAKE_BINARY_DIR}/_deps/extrae-build"
  INSTALL_DIR "${EXTRAE_PREFIX}"

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

  UPDATE_DISCONNECTED 1
  BUILD_ALWAYS OFF
)

# These directories may not exist until extrae_ep installs. Create them so
# imported-target include validation succeeds at configure/generate time.
file(MAKE_DIRECTORY "${EXTRAE_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${EXTRAE_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

add_library(extrae::extrae STATIC IMPORTED GLOBAL)
set_target_properties(extrae::extrae PROPERTIES
  IMPORTED_LOCATION "${EXTRAE_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${EXTRAE_INCLUDE_DIR}"
)

add_dependencies(extrae::extrae extrae_ep)
