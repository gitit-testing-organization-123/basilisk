foreach(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL)
  if(DEFINED ${var})
    string(REGEX REPLACE "(^|[ \t])-DNDEBUG([ \t]|$)" " " ${var} "${${var}}")
    string(STRIP "${${var}}" ${var})
  endif()
endforeach()

# --- grammar --------------------------------------------------------

add_executable(qcc-grammar ast/grammar.c)

# Fix for redhat based systems where the default is to build a PIE
# executable, which we do not want for this utility.
target_compile_options(qcc-grammar PRIVATE -fno-PIE)
target_link_options(qcc-grammar PRIVATE -no-pie)

# --- generated grammar.h --------------------------------------------

set(YACC_IN   "${CMAKE_CURRENT_SOURCE_DIR}/ast/basilisk.yacc")
set(YACC_OUT  "${CMAKE_CURRENT_BINARY_DIR}/ast/grammar.h")

add_custom_command(
  OUTPUT "${YACC_OUT}"
  # Ensure the output directory exists (first build / clean build)
  COMMAND "${CMAKE_COMMAND}" -E make_directory
          "${CMAKE_CURRENT_BINARY_DIR}/ast"
  # Run the *built* grammar tool (not whatever is on PATH)
  COMMAND $<TARGET_FILE:qcc-grammar> < "${YACC_IN}" > "${YACC_OUT}"
  DEPENDS qcc-grammar "${YACC_IN}"
  # BYPRODUCTS "${YACC_OUT}"
  COMMENT "Generating grammar.h from basilisk.yacc"
  VERBATIM
)

# Phony target to make ordering explicit and easy to debug
add_custom_target(qcc-grammar-utility DEPENDS "${YACC_OUT}")

# --- qcc --------------------------------------------------

add_executable(qcc
  qcc.c
  include.c
  postproc.c
  ast/ast.c
  ast/tokens.c
  ast/basilisk.c
  ast/translate.c
  ast/allocator.c
  ast/faststack.c
  ast/stencil.c
  ast/types.c
  ast/references.c
  ast/kernels.c
  ast/check.c
  ast/interpreter/interpreter.c
  "${YACC_OUT}"   # keep listed so the file-level graph is correct
)

# Make sure qcc waits for grammar.h generation
add_dependencies(qcc qcc-grammar)

# Tell CMake this header is generated (helps IDEs and some generators)
set_source_files_properties("${YACC_OUT}" PROPERTIES GENERATED TRUE)

add_executable(basilisk::qcc ALIAS qcc)

target_include_directories(qcc PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/ast"
  "${CMAKE_CURRENT_BINARY_DIR}/ast"   # where grammar.h is generated
  "${CMAKE_CURRENT_SOURCE_DIR}/ast/interpreter"
)

target_compile_definitions(qcc PRIVATE
  LIBDIR=\"${LIBDIR}\"
  CC99=\"${CC99}\"
  CPP99=\"${CPP99}\"
  CADNACC=\"${CADNACC}\"
  BASILISK=\"${BASILISK}\"
)

message(STATUS "qcc using ${BASILISK}")

target_link_libraries(qcc PRIVATE m)

# --- qcc-toolchain --------------------------------------------------

add_executable(qcc-toolchain
  qcc.c
  include.c
  postproc.c
  ast/ast.c
  ast/tokens.c
  ast/basilisk.c
  ast/translate.c
  ast/allocator.c
  ast/faststack.c
  ast/stencil.c
  ast/types.c
  ast/references.c
  ast/kernels.c
  ast/check.c
  ast/interpreter/interpreter.c
  "${YACC_OUT}"   # keep listed so the file-level graph is correct
)

# Make sure qcc waits for grammar.h generation
add_dependencies(qcc-toolchain qcc-grammar)

# Tell CMake this header is generated (helps IDEs and some generators)
set_source_files_properties("${YACC_OUT}" PROPERTIES GENERATED TRUE)

add_executable(basilisk::qcc-toolchain ALIAS qcc-toolchain)

target_include_directories(qcc-toolchain PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/ast"
  "${CMAKE_CURRENT_BINARY_DIR}/ast"   # where grammar.h is generated
  "${CMAKE_CURRENT_SOURCE_DIR}/ast/interpreter"
)

target_compile_definitions(qcc-toolchain PRIVATE
  LIBDIR=\"${LIBDIR_SOURCE}\"
  CC99=\"${CC99}\"
  CPP99=\"${CPP99}\"
  CADNACC=\"${CADNACC}\"
  BASILISK=\"${BASILISK_SOURCE}\"
)

target_compile_options(qcc-toolchain PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-O0>
  $<$<COMPILE_LANGUAGE:CXX>:-O0>
)

message(STATUS "qcc-toolchain using ${BASILISK_SOURCE}")

target_link_libraries(qcc-toolchain PRIVATE m)

# --- gl -------------------------------------------------------------------

add_subdirectory(gl)

# --- bview2D --------------------------------------------------

set(BVIEW2D_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bview2D_gen")
file(MAKE_DIRECTORY "${BVIEW2D_GEN_DIR}")

add_custom_command(
  OUTPUT "${BVIEW2D_GEN_DIR}/_bview.c"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${BVIEW2D_GEN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" "${BVIEW2D_GEN_DIR}/bview.c"
  COMMAND $<TARGET_FILE:qcc-toolchain>
    "bview.c"
    -I"${CMAKE_SOURCE_DIR}"
    -source
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" qcc-toolchain
  WORKING_DIRECTORY "${BVIEW2D_GEN_DIR}"
)

add_executable(bview2D "${BVIEW2D_GEN_DIR}/_bview.c")

set_target_properties(bview2D PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

target_include_directories(bview2D PRIVATE
  "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(bview2D PRIVATE
  fb_tiny
  glutils
  wsserver
  m
)

add_executable(basilisk::bview2D ALIAS bview2D)

# --- bview3D --------------------------------------------------

set(BVIEW3D_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bview3D_gen")
file(MAKE_DIRECTORY "${BVIEW3D_GEN_DIR}")

add_custom_command(
  OUTPUT "${BVIEW3D_GEN_DIR}/_bview.c"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${BVIEW3D_GEN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" "${BVIEW3D_GEN_DIR}/bview.c"
  COMMAND $<TARGET_FILE:qcc-toolchain>
    "bview.c"
    -I"${CMAKE_SOURCE_DIR}"
    -grid=octree
    -source
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" qcc-toolchain
  WORKING_DIRECTORY "${BVIEW3D_GEN_DIR}"
)

add_executable(bview3D "${BVIEW3D_GEN_DIR}/_bview.c")

set_target_properties(bview3D PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

target_include_directories(bview3D PRIVATE
  "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(bview3D PRIVATE
  fb_tiny
  glutils
  wsserver
  m
)

add_executable(basilisk::bview3D ALIAS bview3D)

# --- bview2Dm --------------------------------------------------

set(BVIEW2DM_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bview2Dm_gen")
file(MAKE_DIRECTORY "${BVIEW2DM_GEN_DIR}")

add_custom_command(
  OUTPUT "${BVIEW2DM_GEN_DIR}/_bview.c"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${BVIEW2DM_GEN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" "${BVIEW2DM_GEN_DIR}/bview.c"
  COMMAND $<TARGET_FILE:qcc-toolchain>
    "bview.c"
    -I"${CMAKE_SOURCE_DIR}"
    -grid=multigrid
    -source
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bview.c" qcc-toolchain
  WORKING_DIRECTORY "${BVIEW2DM_GEN_DIR}"
)

add_executable(bview2Dm "${BVIEW2DM_GEN_DIR}/_bview.c")

set_target_properties(bview2Dm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

target_include_directories(bview2Dm PRIVATE
  "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(bview2Dm PRIVATE
  fb_tiny
  glutils
  wsserver
  m
)

add_executable(basilisk::bview2Dm ALIAS bview2Dm)
