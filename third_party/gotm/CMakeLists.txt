include(ExternalProject)
include(GNUInstallDirs)

set(GOTM_PREFIX "${CMAKE_BINARY_DIR}/_deps/gotm-install"
  CACHE PATH "GOTM install prefix")
set(GOTM_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/gotm-src"
  CACHE PATH "GOTM source tree")
set(GOTM_LIBRARY "${GOTM_PREFIX}/lib/libgotm.a"
  CACHE FILEPATH "GOTM static library")
set(GOTM_INCLUDE_DIR "${GOTM_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/gotm"
  CACHE PATH "GOTM include directory")
set(GOTM_LIBRARY_DIR "${GOTM_PREFIX}/lib")

ExternalProject_Add(gotm_ep
  GIT_REPOSITORY https://github.com/gitit-testing-organization-123/gotm.git

  SOURCE_DIR  "${GOTM_SOURCE_DIR}"
  BINARY_DIR  "${CMAKE_BINARY_DIR}/_deps/gotm-build"
  SOURCE_SUBDIR src
  INSTALL_DIR "${GOTM_PREFIX}"

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DGOTM_USE_FABM=OFF
    -DGOTM_USE_NetCDF=ON

  UPDATE_DISCONNECTED 1
  BUILD_ALWAYS OFF
)

# These directories may not exist until gotm_ep installs. Create them so
# imported-target include validation succeeds at configure/generate time.
file(MAKE_DIRECTORY "${GOTM_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${GOTM_LIBRARY_DIR}")

set(GOTM_STATIC_LIBRARIES
  "${GOTM_LIBRARY_DIR}/libgotm.a"
  "${GOTM_LIBRARY_DIR}/libairsea.a"
  "${GOTM_LIBRARY_DIR}/libmeanflow.a"
  "${GOTM_LIBRARY_DIR}/libobservations.a"
  "${GOTM_LIBRARY_DIR}/libturbulence.a"
  "${GOTM_LIBRARY_DIR}/libinput.a"
  "${GOTM_LIBRARY_DIR}/libutil.a"
  "${GOTM_LIBRARY_DIR}/liboutput_manager.a"
  "${GOTM_LIBRARY_DIR}/libturbulence.a"
  "${GOTM_LIBRARY_DIR}/liboutput_manager.a"
  "${GOTM_LIBRARY_DIR}/libmeanflow.a"
  "${GOTM_LIBRARY_DIR}/libairsea.a"
  "${GOTM_LIBRARY_DIR}/libobservations.a"
  "${GOTM_LIBRARY_DIR}/libinput.a"
  "${GOTM_LIBRARY_DIR}/libutil.a"
)

find_program(GOTM_NETCDF_CONFIG_EXECUTABLE nf-config REQUIRED)
execute_process(
  COMMAND "${GOTM_NETCDF_CONFIG_EXECUTABLE}" --flibs
  OUTPUT_VARIABLE GOTM_NETCDF_FLIBS_RAW
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(GOTM_NETCDF_FLIBS UNIX_COMMAND "${GOTM_NETCDF_FLIBS_RAW}")

set(GOTM_FORTRAN_LINK_FLAGS)
foreach(link_dir IN LISTS CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES)
  list(APPEND GOTM_FORTRAN_LINK_FLAGS "-L${link_dir}")
endforeach()

add_library(GOTM::gotm INTERFACE IMPORTED GLOBAL)
set_target_properties(GOTM::gotm PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${GOTM_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${GOTM_STATIC_LIBRARIES};${GOTM_NETCDF_FLIBS};${GOTM_FORTRAN_LINK_FLAGS};${CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES}"
)

add_dependencies(GOTM::gotm gotm_ep)
