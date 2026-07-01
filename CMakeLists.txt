cmake_minimum_required(VERSION 3.5...4.1)
project(basilisk 
  VERSION 0.0.1
  LANGUAGES C CXX
)

# Project settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BASILISK_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include(GNUInstallDirs)


# --- config ---------------------------------------------------------

include(${BASILISK_ROOT}/cmake/detect_compiler.cmake)
detect_c_compiler(COMP)

message(STATUS " → C compiler ID:      ${COMP_ID}")
message(STATUS " → C compiler version: ${COMP_VERSION}")
message(STATUS " → C compiler path:    ${COMP_PATH}")

# ——— C99 flags ———

#
# These are used when qcc is not given the -source flag and pipes the 
# transpiled code to another compiler. These definitions are taken 
# directly from src/config.* where * denotes various compilers. Most
# users will use GCC.
#

if(COMP_ID STREQUAL GNU)

  set(C99_FLAGS
      "-std=c99 -D_XOPEN_SOURCE=700"
      CACHE STRING "Flags to pass to a C99 compiler"
  )

  set(CC99
      "${CMAKE_C_COMPILER} ${C99_FLAGS} -D_GNU_SOURCE=1 -pedantic -Wno-unused-result -Wno-overlength-strings -fno-diagnostics-show-caret"
      CACHE STRING "Command line used by qcc to invoke a C99 compiler"
  )

  # ——— CADNA compiler and libraries ———
  set(CADNACC
      "clang -D_CADNA=1 -x c++ -m64 -Wno-unused-function -Wno-unused-result -Wno-c++11-compat-deprecated-writable-strings -Wno-address-of-array-temporary"
      CACHE STRING "Compiler flags for building with CADNA"
  )

  set(CADNALIBS
      "-lcadnaC -lstdc++"
      CACHE STRING "Libraries to link against when using CADNA"
  )

  mark_as_advanced(C99_FLAGS CC99 CADNACC CADNALIBS)
    
elseif(COMP_ID STREQUAL "Clang")

  set(C99_FLAGS
      "-std=c99 -D_XOPEN_SOURCE=700"
      CACHE STRING "Flags to pass to a C99 compiler"
  )

  set(CC99
      "${CMAKE_C_COMPILER} ${C99_FLAGS} -pedantic  -Wno-unused-result"
      CACHE STRING "Command line used by qcc to invoke a C99 compiler"
  )

  # ——— CADNA compiler and libraries ———
  set(CADNACC
      "clang -D_CADNA=1 -x c++ -m64 -Wno-unused-function -Wno-unused-result -Wno-c++11-compat-deprecated-writable-strings -Wno-address-of-array-temporary"
      CACHE STRING "Compiler flags for building with CADNA"
  )

  set(CADNALIBS
      "-lcadnaC -lstdc++"
      CACHE STRING "Libraries to link against when using CADNA"
  )

  mark_as_advanced(C99_FLAGS CC99 CADNACC CADNALIBS)

elseif(COMP_ID STREQUAL "Intel")

  set(CC99
      "icc -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE"
      CACHE STRING "Command line used by qcc to invoke a C99 compiler"
  )
  mark_as_advanced(CC99)

endif()

# When qcc is built, it needs a compile-time macro definiing the home
# of the basilisk source code: The consequence is that we cannot relocate
# our library without also recompiling qcc to point to this new location.
#
# Therefore, we create two qcc binaries: One points to the source tree, which 
# should be used for "internal" toolchain build steps when included in other 
# projects. The other is an "install" executable which will expect the source 
# to live at the desired include path

set(BASILISK "${CMAKE_INSTALL_FULL_INCLUDEDIR}/basilisk/"
    CACHE PATH "Root for Basilisk includes used by qcc")
set(LIBDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}/basilisk/"
    CACHE PATH "Root for Basilisk libraries used by qcc")

set(BASILISK_SOURCE "${PROJECT_SOURCE_DIR}/src/"
    CACHE PATH "Root for Basilisk includes used by qcc-toolchain")
set(LIBDIR_SOURCE "${PROJECT_SOURCE_DIR}/src/"
    CACHE PATH "Root for Basilisk libraries used by qcc-toolchain")

mark_as_advanced(BASILISK LIBDIR BASILISK_SOURCE LIBDIR_SOURCE)

#
# Now, we recurse into the source directory
#

add_subdirectory(src)

#
# Installation + CMake package export
#
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(basilisk_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/basilisk"
    CACHE PATH "CMake package config dir for basilisk")

install(TARGETS
  qcc
  bview2D
  bview2Dm
  bview3D
  fb_tiny
  glutils
  tinyrenderer
  wsserver
  EXPORT basiliskTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# --- Install headers / sources needed as includes --------------------------------
install(DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/basilisk"
  FILES_MATCHING
  PATTERN "*"
)

# --- Export targets (basiliskTargets.cmake) ---------------------------------------
install(EXPORT basiliskTargets
  FILE basiliskTargets.cmake
  NAMESPACE basilisk::
  DESTINATION "${basilisk_INSTALL_CMAKEDIR}"
)

# --- Install helper CMake modules -------------------------------------------------
install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/basilisk_add_executable.cmake"
  DESTINATION "${basilisk_INSTALL_CMAKEDIR}"
)

# --- Generate & install basiliskConfig.cmake (+ optional version file) ------------
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/basiliskConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/basiliskConfig.cmake"
  INSTALL_DESTINATION "${basilisk_INSTALL_CMAKEDIR}"
)

# Optional but recommended if you set project(VERSION ...)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/basiliskConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/basiliskConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/basiliskConfigVersion.cmake"
  DESTINATION "${basilisk_INSTALL_CMAKEDIR}"
)
