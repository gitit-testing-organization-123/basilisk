if (BASILISK_USE_KDT)
  add_library(kdt STATIC
    kdt.c
  )

  add_library(basilisk::kdt ALIAS kdt)

  target_compile_definitions(kdt
    PRIVATE
      _FILE_OFFSET_BITS=64
  )

  target_include_directories(kdt
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk/kdt>
  )

  target_link_libraries(kdt
    PUBLIC
      m
  )

  add_executable(xyz2kdt
    xyz2kdt.c
  )

  target_link_libraries(xyz2kdt
    PRIVATE
      kdt
      m
  )

  add_executable(kdtquery
    kdtquery.c
  )

  target_link_libraries(kdtquery
    PRIVATE
      kdt
      m
  )

  install(TARGETS kdt xyz2kdt kdtquery
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()
