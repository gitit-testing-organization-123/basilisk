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
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT gpu-common
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-common
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-common
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
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT gpu-glsl
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-glsl
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-glsl
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
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT gpu-cuda
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-cuda
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-cuda
  )
endif()

if (BASILISK_USE_HIP)
  find_package(hip REQUIRED CONFIG)
  find_package(hiprtc REQUIRED CONFIG)

  add_library(hipamd STATIC
    hip/hip.c
  )

  add_library(basilisk::hipamd ALIAS hipamd)

  target_compile_definitions(hipamd PRIVATE
    SINGLE_PRECISION
    __HIP_PLATFORM_AMD__
  )

  target_include_directories(hipamd
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:include/basilisk>
      $<INSTALL_INTERFACE:include/basilisk/grid>
  )

  target_link_libraries(hipamd
    PUBLIC
      basilisk::errors
      hip::host
      hiprtc::hiprtc
      m
  )

  install(TARGETS hipamd
    EXPORT basiliskTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT gpu-hip
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-hip
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT gpu-hip
  )
endif()
