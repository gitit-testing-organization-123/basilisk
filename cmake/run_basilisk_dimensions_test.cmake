foreach(required_var IN ITEMS
    TEST_NAME
    TEST_DIMENSIONS_NAME
    TEST_SOURCE_FILE
    TEST_WORK_DIR
    TEST_SOURCE_DIR
    TEST_QCC
    TEST_BASILISK_SOURCE_DIR)
  if(NOT DEFINED ${required_var})
    message(FATAL_ERROR "${required_var} is required")
  endif()
endforeach()

foreach(default_var IN ITEMS
    TEST_REF_FILE
    TEST_TIMEOUT
    TEST_DEFINES
    TEST_STAGE_FILES
    TEST_INCLUDES
    TEST_QCC_FLAGS
    TEST_OPENGLIBS)
  if(NOT DEFINED ${default_var})
    set(${default_var} "")
  endif()
endforeach()

if(NOT TEST_TIMEOUT)
  set(TEST_TIMEOUT 10800)
endif()

string(REPLACE "|" ";" TEST_DEFINES "${TEST_DEFINES}")
string(REPLACE "|" ";" TEST_STAGE_FILES "${TEST_STAGE_FILES}")
string(REPLACE "|" ";" TEST_INCLUDES "${TEST_INCLUDES}")
string(REPLACE "|" ";" TEST_QCC_FLAGS "${TEST_QCC_FLAGS}")

function(_basilisk_dump_file label path)
  if(EXISTS "${path}")
    file(READ "${path}" contents)
    if(contents)
      message(STATUS "${label}:")
      message(STATUS "${contents}")
    endif()
  endif()
endfunction()

function(_basilisk_stage_link source destination)
  if(IS_SYMLINK "${destination}")
    return()
  endif()
  if(EXISTS "${destination}")
    file(REMOVE "${destination}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "${source}" "${destination}"
    RESULT_VARIABLE link_result
  )
  if(NOT link_result EQUAL 0)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${destination}"
      RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
      message(FATAL_ERROR "could not stage '${source}' as '${destination}'")
    endif()
  endif()
endfunction()

file(MAKE_DIRECTORY "${TEST_WORK_DIR}")

set(staged_source "${TEST_WORK_DIR}/${TEST_DIMENSIONS_NAME}.c")
set(dimensions_output "${TEST_WORK_DIR}/${TEST_DIMENSIONS_NAME}.dims")
set(dimensions_compare "${TEST_WORK_DIR}/${TEST_DIMENSIONS_NAME}.dims.compare")
set(dimensions_ref_compare "${TEST_WORK_DIR}/${TEST_DIMENSIONS_NAME}.dims.ref.compare")
set(qcc_output "${TEST_WORK_DIR}/${TEST_DIMENSIONS_NAME}.s")
set(qcc_stdout "${TEST_WORK_DIR}/dimensions.out")
set(qcc_stderr "${TEST_WORK_DIR}/dimensions.err")

file(REMOVE
  "${dimensions_output}"
  "${dimensions_compare}"
  "${dimensions_ref_compare}"
  "${qcc_output}"
  "${qcc_stdout}"
  "${qcc_stderr}"
)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${TEST_SOURCE_FILE}" "${staged_source}"
  RESULT_VARIABLE stage_result
)
if(NOT stage_result EQUAL 0)
  message(FATAL_ERROR
    "${TEST_NAME}: could not stage '${TEST_SOURCE_FILE}' as '${staged_source}'")
endif()

foreach(stage_file IN LISTS TEST_STAGE_FILES)
  if(NOT IS_ABSOLUTE "${stage_file}")
    set(stage_file "${TEST_SOURCE_DIR}/${stage_file}")
  endif()
  if(NOT EXISTS "${stage_file}")
    message(FATAL_ERROR "${TEST_NAME}: missing staged file '${stage_file}'")
  endif()

  get_filename_component(stage_name "${stage_file}" NAME)
  _basilisk_stage_link("${stage_file}" "${TEST_WORK_DIR}/${stage_name}")
endforeach()

set(qcc_define_flags)
foreach(definition IN LISTS TEST_DEFINES)
  if(definition MATCHES "^-D")
    list(APPEND qcc_define_flags "${definition}")
  else()
    list(APPEND qcc_define_flags "-D${definition}")
  endif()
endforeach()

set(qcc_include_flags
  "-I${TEST_SOURCE_DIR}"
  "-I${TEST_BASILISK_SOURCE_DIR}"
)
foreach(include_dir IN LISTS TEST_INCLUDES)
  if(include_dir MATCHES "^-I")
    list(APPEND qcc_include_flags "${include_dir}")
  else()
    list(APPEND qcc_include_flags "-I${include_dir}")
  endif()
endforeach()

set(test_env
  "BASILISK=${TEST_BASILISK_SOURCE_DIR}/src"
  "BASILISK_LIBDIR=${TEST_BASILISK_SOURCE_DIR}/src"
  "OPENGLIBS=${TEST_OPENGLIBS}"
)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_env}
    "${TEST_QCC}"
    -autolink
    -nolineno
    -dimensions=dims
    -non-finite
    ${qcc_define_flags}
    ${qcc_include_flags}
    ${TEST_QCC_FLAGS}
    "${TEST_DIMENSIONS_NAME}.c"
    -o "${qcc_output}"
    -lm
  WORKING_DIRECTORY "${TEST_WORK_DIR}"
  OUTPUT_FILE "${qcc_stdout}"
  ERROR_FILE "${qcc_stderr}"
  TIMEOUT "${TEST_TIMEOUT}"
  RESULT_VARIABLE qcc_result
)

if(NOT qcc_result EQUAL 0)
  _basilisk_dump_file("${TEST_NAME} qcc stdout" "${qcc_stdout}")
  _basilisk_dump_file("${TEST_NAME} qcc stderr" "${qcc_stderr}")
  message(FATAL_ERROR "${TEST_NAME}: qcc dimensions check failed")
endif()

if(TEST_REF_FILE AND EXISTS "${TEST_REF_FILE}")
  if(NOT EXISTS "${dimensions_output}")
    _basilisk_dump_file("${TEST_NAME} qcc stdout" "${qcc_stdout}")
    _basilisk_dump_file("${TEST_NAME} qcc stderr" "${qcc_stderr}")
    message(FATAL_ERROR
      "${TEST_NAME}: qcc did not produce '${dimensions_output}'")
  endif()

  file(READ "${dimensions_output}" dimensions_log)
  file(READ "${TEST_REF_FILE}" dimensions_ref_log)
  string(REPLACE "]  src/" "]  /src/" dimensions_log "${dimensions_log}")
  string(REPLACE "]  src/" "]  /src/" dimensions_ref_log "${dimensions_ref_log}")
  file(WRITE "${dimensions_compare}" "${dimensions_log}")
  file(WRITE "${dimensions_ref_compare}" "${dimensions_ref_log}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${dimensions_compare}" "${dimensions_ref_compare}"
    RESULT_VARIABLE compare_result
  )
  if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR
      "${TEST_NAME}: dimensions differ from ${TEST_REF_FILE}; "
      "actual dimensions are ${dimensions_output}")
  endif()
endif()
