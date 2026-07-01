include(ExternalProject)
include(GNUInstallDirs)

set(CVMIX_PREFIX "${CMAKE_BINARY_DIR}/_deps/cvmix-install"
  CACHE PATH "CVMix install prefix")
set(CVMIX_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/cvmix-src"
  CACHE PATH "CVMix source tree")
set(CVMIX_LIBRARY "${CVMIX_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libcvmix.a"
  CACHE FILEPATH "CVMix static library")
set(CVMIX_INCLUDE_DIR "${CVMIX_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/cvmix"
  CACHE PATH "CVMix include directory")

ExternalProject_Add(cvmix_ep
  GIT_REPOSITORY https://github.com/CVMix/CVMix-src
  GIT_TAG        v1.0.0

  SOURCE_DIR  "${CVMIX_SOURCE_DIR}"
  BINARY_DIR  "${CMAKE_BINARY_DIR}/_deps/cvmix-build"
  INSTALL_DIR "${CVMIX_PREFIX}"

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCVMIX_BUILD_STATIC_LIBS=ON
    -DCVMIX_BUILD_SHARED_LIBS=OFF
    -DCVMIX_BUILD_DRIVER=OFF
    -DCVMIX_USE_NetCDF=OFF

  UPDATE_DISCONNECTED 1
  BUILD_ALWAYS OFF
)

# These directories may not exist until cvmix_ep installs. Create them so
# imported-target include validation succeeds at configure/generate time.
file(MAKE_DIRECTORY "${CVMIX_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${CVMIX_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

add_library(CVMix::cvmix STATIC IMPORTED GLOBAL)
set_target_properties(CVMix::cvmix PROPERTIES
  IMPORTED_LOCATION "${CVMIX_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${CVMIX_INCLUDE_DIR}"
)

add_dependencies(CVMix::cvmix cvmix_ep)
