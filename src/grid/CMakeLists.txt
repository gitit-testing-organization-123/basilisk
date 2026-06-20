option(BASILISK_USE_CUDA "Build the CUDA GPU backend" OFF)
option(BASILISK_USE_GLSL "Build the OpenGL/GLSL GPU backend" OFF)
option(BASILISK_USE_HIP "Build the HIP GPU backend" OFF)

if (BASILISK_USE_GLSL OR BASILISK_USE_CUDA OR BASILISK_USE_HIP)
  add_library(errors STATIC
    gpu/errors.c
  )

  add_library(basilisk::errors ALIAS errors)

  target_include_directories(errors
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk>
      $<INSTALL_INTERFACE:include/basilisk/grid>
  )

  install(TARGETS errors
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if (BASILISK_USE_GLSL)
  find_package(glfw3 REQUIRED)

  add_library(gpu STATIC
    gpu/glad.c
    gpu/opengl.c
  )

  add_library(basilisk::gpu ALIAS gpu)

  target_compile_definitions(gpu PRIVATE
    SINGLE_PRECISION
  )

  target_include_directories(gpu
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk>
      $<INSTALL_INTERFACE:include/basilisk/grid>
  )

  target_link_libraries(gpu
    PUBLIC
      basilisk::errors
      glfw
      ${CMAKE_DL_LIBS}
      m
  )

  install(TARGETS gpu
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if (BASILISK_USE_CUDA)
  find_package(CUDAToolkit REQUIRED)

  add_library(buda STATIC
    cuda/cuda.c
  )

  add_library(basilisk::buda ALIAS buda)

  target_compile_definitions(buda PRIVATE
    SINGLE_PRECISION
  )

  target_include_directories(buda
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk>
      $<INSTALL_INTERFACE:include/basilisk/grid>
  )

  target_link_libraries(buda
    PUBLIC
      basilisk::errors
      CUDA::cuda_driver
      CUDA::nvrtc
      m
  )

  install(TARGETS buda
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if (BASILISK_USE_HIP)
  find_package(hip REQUIRED CONFIG)
  find_package(hiprtc REQUIRED CONFIG)

  add_library(basilisk_hip STATIC
    hip/hip.c
  )

  add_library(basilisk::hip ALIAS basilisk_hip)

  set_target_properties(basilisk_hip PROPERTIES
    EXPORT_NAME hip
    OUTPUT_NAME hip
  )

  target_compile_definitions(basilisk_hip PRIVATE
    SINGLE_PRECISION
    __HIP_PLATFORM_AMD__
  )

  target_include_directories(basilisk_hip
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk>
      $<INSTALL_INTERFACE:include/basilisk/grid>
  )

  target_link_libraries(basilisk_hip
    PUBLIC
      basilisk::errors
      hip::host
      hiprtc::hiprtc
      m
  )

  install(TARGETS basilisk_hip
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif()
