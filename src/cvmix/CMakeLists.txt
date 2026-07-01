if (BASILISK_USE_CVMIX)
  find_program(AWK_EXECUTABLE NAMES gawk awk REQUIRED)

  set(_cvmix_wrappers
    kinds_and_types
    background
    convection
    ddiff
    kpp
    math
    put_get
    shear
    tidal
    utils
  )

  set(_cvmix_generated_headers)
  set(_cvmix_generated_sources)

  foreach(_wrapper IN LISTS _cvmix_wrappers)
    set(_source "${CVMIX_SOURCE_DIR}/src/shared/cvmix_${_wrapper}.F90")
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_wrapper}.h")
    set(_fortran "${CMAKE_CURRENT_BINARY_DIR}/${_wrapper}.F90")

    add_custom_command(
      OUTPUT "${_header}" "${_fortran}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
      COMMAND /bin/sh -c
        "tr '[:upper:]' '[:lower:]' < \"${_source}\" | \"${AWK_EXECUTABLE}\" -f \"${CMAKE_CURRENT_SOURCE_DIR}/comments.awk\" | \"${AWK_EXECUTABLE}\" -f \"${CMAKE_CURRENT_SOURCE_DIR}/cvmix.awk\" -vbase=\"${_wrapper}\" -vsourcefile=\"${_source}\" > \"${_header}\""
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/comments.awk"
        "${CMAKE_CURRENT_SOURCE_DIR}/cvmix.awk"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMENT "Generating CVMix C/Fortran wrapper for ${_wrapper}"
      VERBATIM
    )

    list(APPEND _cvmix_generated_headers "${_header}")
    list(APPEND _cvmix_generated_sources "${_fortran}")
  endforeach()

  add_library(cvmixc STATIC
    common.F90
    ${_cvmix_generated_sources}
    ${_cvmix_generated_headers}
  )

  add_library(basilisk::cvmixc ALIAS cvmixc)

  if (TARGET cvmix_ep)
    add_dependencies(cvmixc cvmix_ep)
  endif()

  target_include_directories(cvmixc
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
      $<INSTALL_INTERFACE:include/basilisk/cvmix>
    PRIVATE
      "${CVMIX_INCLUDE_DIR}"
  )

  target_link_libraries(cvmixc
    PUBLIC
      CVMix::cvmix
  )

  install(TARGETS cvmixc
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT cvmix
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT cvmix
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT cvmix
  )

  install(FILES ${_cvmix_generated_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/basilisk/cvmix"
    COMPONENT cvmix
  )

  install(FILES "${CVMIX_LIBRARY}"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RENAME "libcvmix.a"
    COMPONENT cvmix
  )
endif()
