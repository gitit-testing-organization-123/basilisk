add_subdirectory(tinyrenderer)

find_package(Threads REQUIRED)

add_library(glutils STATIC
  trackball.c
  utils.c
  polygonize.c
  og_stroke_mono_roman.c
  parser.tab.c
  TinyPngOut.c
)

target_include_directories(glutils
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../wsServer/include>
    $<INSTALL_INTERFACE:include/basilisk/gl>
    $<INSTALL_INTERFACE:include/basilisk>
    $<INSTALL_INTERFACE:include/basilisk/wsServer/include>
)

target_link_libraries(glutils
  PUBLIC
    m
)

add_library(fb_tiny STATIC
  fb_tiny.c
)

target_include_directories(fb_tiny
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/basilisk/gl>
)

target_link_libraries(fb_tiny
  PUBLIC
    glutils
    tinyrenderer
    m
)

add_library(wsserver STATIC
  ../wsServer/src/ws.c
  ../wsServer/src/base64/base64.c
  ../wsServer/src/handshake/handshake.c
  ../wsServer/src/sha1/sha1.c
)

target_include_directories(wsserver
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../wsServer/include>
    $<INSTALL_INTERFACE:include/basilisk/wsServer/include>
)

target_link_libraries(wsserver
  PUBLIC
    Threads::Threads
)
