if (BASILISK_USE_PPR)
  add_library(ppr STATIC
    ppr_1d.f90
    ppr.f90
  )

  add_library(basilisk::ppr ALIAS ppr)

  target_include_directories(ppr
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk/ppr>
  )

  set_target_properties(ppr PROPERTIES
    Fortran_PREPROCESS ON
    Fortran_MODULE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/modules"
  )

  install(TARGETS ppr
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ppr
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT ppr
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT ppr
  )
endif()
