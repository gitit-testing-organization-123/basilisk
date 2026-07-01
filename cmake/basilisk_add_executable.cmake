function(_add_basilisk_executable)
  set(options MPI OPENMP)
  set(one_value_args NAME SOURCE)
  set(multi_value_args DEFINES INCLUDES LIBS QCC_FLAGS)

  cmake_parse_arguments(BAE
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
  )

  if(BAE_UNPARSED_ARGUMENTS)
    list(LENGTH BAE_UNPARSED_ARGUMENTS unparsed_count)
    if(unparsed_count GREATER 1)
      message(FATAL_ERROR
        "add_basilisk_executable accepts at most one positional source; "
        "use NAME and SOURCE for keyword arguments")
    endif()
    if(BAE_SOURCE)
      message(FATAL_ERROR
        "add_basilisk_executable got SOURCE both positionally and by keyword")
    endif()
    list(GET BAE_UNPARSED_ARGUMENTS 0 BAE_SOURCE)
  endif()

  if(NOT BAE_SOURCE AND BAE_NAME)
    set(BAE_SOURCE "${BAE_NAME}.c")
  endif()

  if(NOT BAE_SOURCE)
    message(FATAL_ERROR "add_basilisk_executable requires a source file")
  endif()

  if(NOT BAE_NAME)
    get_filename_component(BAE_NAME "${BAE_SOURCE}" NAME_WE)
  endif()

  get_filename_component(source_abs "${BAE_SOURCE}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  set(qcc_define_flags)
  set(target_definitions)
  
  foreach(definition IN LISTS BAE_DEFINES)
    if(definition MATCHES "^-D")
      list(APPEND qcc_define_flags "${definition}")
      string(REGEX REPLACE "^-D" "" definition "${definition}")
    else()
      list(APPEND qcc_define_flags "-D${definition}")
    endif()
    list(APPEND target_definitions "${definition}")
  endforeach()

  if(BAE_MPI)
    find_package(MPI REQUIRED COMPONENTS C)
    list(APPEND qcc_define_flags "-D_MPI=1")
    list(APPEND target_definitions "_MPI=1")
    list(APPEND BAE_LIBS MPI::MPI_C)
  endif()

  if(BAE_OPENMP)
    find_package(OpenMP REQUIRED COMPONENTS C)
    list(APPEND BAE_QCC_FLAGS "-fopenmp")
    list(APPEND BAE_LIBS OpenMP::OpenMP_C)
  endif()

  set(qcc_include_flags
    "-I${CMAKE_CURRENT_SOURCE_DIR}"
    "-I${CMAKE_SOURCE_DIR}"
  )
  set(target_include_dirs
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}"
  )
  foreach(include_dir IN LISTS BAE_INCLUDES)
    if(include_dir MATCHES "^-I")
      list(APPEND qcc_include_flags "${include_dir}")
      string(REGEX REPLACE "^-I" "" include_dir "${include_dir}")
    else()
      list(APPEND qcc_include_flags "-I${include_dir}")
    endif()
    list(APPEND target_include_dirs "${include_dir}")
  endforeach()

  set(qcc_input_name "${BAE_NAME}.qcc")
  set(copied_c "${CMAKE_CURRENT_BINARY_DIR}/${qcc_input_name}.c")
  set(output_c "${CMAKE_CURRENT_BINARY_DIR}/_${qcc_input_name}.c")
  set(qcc_flags_file "${CMAKE_CURRENT_BINARY_DIR}/${qcc_input_name}.flags")

  set(qcc_flags_content
    "source=${source_abs}\n"
    "includes=${qcc_include_flags}\n"
    "defines=${qcc_define_flags}\n"
    "qcc_flags=${BAE_QCC_FLAGS}\n"
  )
  file(GENERATE OUTPUT "${qcc_flags_file}" CONTENT "${qcc_flags_content}")

  add_custom_command(
    OUTPUT "${output_c}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${source_abs}" "${copied_c}"
    COMMAND $<TARGET_FILE:basilisk::qcc>
            ${qcc_include_flags}
            ${qcc_define_flags}
            ${BAE_QCC_FLAGS}
            -source
            "${qcc_input_name}.c"
    DEPENDS "${source_abs}" "${qcc_flags_file}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )

  add_executable(${BAE_NAME} "${output_c}")

  target_compile_definitions(${BAE_NAME} PRIVATE ${target_definitions})
  target_include_directories(${BAE_NAME} PRIVATE ${target_include_dirs})
  target_compile_options(${BAE_NAME} PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wno-stringop-overflow>
  )

  target_link_libraries(${BAE_NAME}
    PRIVATE
      m
      ${BAE_LIBS}
  )

  # set_target_properties(${BAE_NAME} PROPERTIES
  #   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  #   BUILD_RPATH "${CMAKE_CURRENT_BINARY_DIR}"
  # )

endfunction()

function(add_basilisk_executable)
  _add_basilisk_executable(${ARGN})
endfunction()

function(add_basilisk_mpi_executable)
  _add_basilisk_executable(MPI ${ARGN})
endfunction()

function(add_basilisk_openmp_executable)
  _add_basilisk_executable(OPENMP ${ARGN})
endfunction()
