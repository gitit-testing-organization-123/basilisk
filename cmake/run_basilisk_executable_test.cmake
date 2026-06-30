foreach(required_var IN ITEMS
    TEST_NAME
    TEST_EXECUTABLE
    TEST_WORK_DIR
    TEST_SOURCE_DIR)
  if(NOT DEFINED ${required_var})
    message(FATAL_ERROR "${required_var} is required")
  endif()
endforeach()

foreach(default_var IN ITEMS
    TEST_REF_FILE
    TEST_TIMEOUT
    TEST_EXECUTOR
    TEST_MPI_RANKS
    TEST_STAGE_FILES
    TEST_STAGE_TEST_FILES
    TEST_BINARY_TEST_DIR
    TEST_EXTRA_PATH)
  if(NOT DEFINED ${default_var})
    set(${default_var} "")
  endif()
endforeach()

if(NOT TEST_TIMEOUT)
  set(TEST_TIMEOUT 10800)
endif()

string(REPLACE "|" ";" TEST_STAGE_FILES "${TEST_STAGE_FILES}")
string(REPLACE "|" ";" TEST_STAGE_TEST_FILES "${TEST_STAGE_TEST_FILES}")
string(REPLACE "|" ";" TEST_EXECUTOR "${TEST_EXECUTOR}")
string(REPLACE "|" ";" TEST_EXTRA_PATH "${TEST_EXTRA_PATH}")

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
file(REMOVE
  "${TEST_WORK_DIR}/out"
  "${TEST_WORK_DIR}/log"
  "${TEST_WORK_DIR}/log.compare"
  "${TEST_WORK_DIR}/log.merged"
  "${TEST_WORK_DIR}/fail"
  "${TEST_WORK_DIR}/warn"
)
file(GLOB stale_rank_logs "${TEST_WORK_DIR}/log-*")
if(stale_rank_logs)
  file(REMOVE ${stale_rank_logs})
endif()
file(GLOB stale_rank_outputs "${TEST_WORK_DIR}/out-*")
if(stale_rank_outputs)
  file(REMOVE ${stale_rank_outputs})
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

foreach(stage_mapping IN LISTS TEST_STAGE_TEST_FILES)
  string(REGEX MATCH "^([^:]+):([^=]+)(=(.+))?$" matched "${stage_mapping}")
  if(NOT matched)
    message(FATAL_ERROR
      "${TEST_NAME}: invalid FILES_FROM_TEST mapping '${stage_mapping}'; "
      "expected producer:path or producer:path=dest")
  endif()

  set(producer_test "${CMAKE_MATCH_1}")
  set(producer_path "${CMAKE_MATCH_2}")
  set(stage_name "${CMAKE_MATCH_4}")
  if(NOT stage_name)
    get_filename_component(stage_name "${producer_path}" NAME)
  endif()

  set(stage_file "${TEST_BINARY_TEST_DIR}/${producer_test}/${producer_path}")
  if(NOT EXISTS "${stage_file}")
    message(FATAL_ERROR
      "${TEST_NAME}: missing producer artifact '${stage_file}' "
      "from test '${producer_test}'")
  endif()

  _basilisk_stage_link("${stage_file}" "${TEST_WORK_DIR}/${stage_name}")
endforeach()

set(run_stdout "${TEST_WORK_DIR}/out")
set(run_stderr "${TEST_WORK_DIR}/log")
set(compare_stderr "${TEST_WORK_DIR}/log.compare")

set(test_command)
if(TEST_EXTRA_PATH)
  if(WIN32)
    set(path_separator ";")
  else()
    set(path_separator ":")
  endif()
  string(REPLACE ";" "${path_separator}" test_extra_path_env "${TEST_EXTRA_PATH}")
  if(DEFINED ENV{PATH} AND NOT "$ENV{PATH}" STREQUAL "")
    set(test_path_env "${test_extra_path_env}${path_separator}$ENV{PATH}")
  else()
    set(test_path_env "${test_extra_path_env}")
  endif()
  list(APPEND test_command "${CMAKE_COMMAND}" -E env "PATH=${test_path_env}")
endif()
if(TEST_EXECUTOR)
  list(APPEND test_command ${TEST_EXECUTOR})
endif()
list(APPEND test_command "${TEST_EXECUTABLE}")

execute_process(
  COMMAND ${test_command}
  WORKING_DIRECTORY "${TEST_WORK_DIR}"
  OUTPUT_FILE "${run_stdout}"
  ERROR_FILE "${run_stderr}"
  TIMEOUT "${TEST_TIMEOUT}"
  RESULT_VARIABLE run_result
)

if(NOT run_result EQUAL 0)
  _basilisk_dump_file("${TEST_NAME} stdout" "${run_stdout}")
  _basilisk_dump_file("${TEST_NAME} stderr" "${run_stderr}")
  message(FATAL_ERROR "${TEST_NAME}: executable failed with status ${run_result}")
endif()

if(EXISTS "${TEST_WORK_DIR}/log-1")
  set(rank0_stderr "${TEST_WORK_DIR}/log-0")
  file(RENAME "${run_stderr}" "${rank0_stderr}")
  file(GLOB rank_logs "${TEST_WORK_DIR}/log-*")
  list(SORT rank_logs)
  file(WRITE "${run_stderr}" "")
  foreach(rank_log IN LISTS rank_logs)
    file(READ "${rank_log}" rank_log_contents)
    file(APPEND "${run_stderr}" "${rank_log_contents}")
  endforeach()
endif()

if(TEST_REF_FILE AND EXISTS "${TEST_REF_FILE}")
  set(compare_input "${run_stderr}")

  file(READ "${compare_input}" compare_log)
  string(REGEX REPLACE
    "WARNING: The convert command is deprecated in IMv7, use \"magick\" instead of \"convert\" or \"magick convert\"\n+"
    ""
    compare_log
    "${compare_log}"
  )
  string(REGEX REPLACE
    "src/output\\.h:[0-9]+: warning: cannot find 'ppm2[a-z0-9]+' or 'ffmpeg'/'avconv'\nsrc/output\\.h:[0-9]+: warning: falling back to raw PPM outputs\n+"
    ""
    compare_log
    "${compare_log}"
  )
  string(REGEX REPLACE
    "sh: (line 1: )?ppm2[a-z0-9]+: command not found\n+"
    ""
    compare_log
    "${compare_log}"
  )
  string(REGEX REPLACE
    "sh: (line 1: )?gfsview-batch[23]D: command not found\n+"
    ""
    compare_log
    "${compare_log}"
  )
  string(REGEX REPLACE
    "([A-Za-z0-9_.+-]+)\\.qcc\\.c:"
    "\\1.c:"
    compare_log
    "${compare_log}"
  )
  if(compare_log AND NOT compare_log MATCHES "\n$")
    string(APPEND compare_log "\n")
  endif()
  file(WRITE "${compare_stderr}" "${compare_log}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${compare_stderr}" "${TEST_REF_FILE}"
    RESULT_VARIABLE compare_result
  )
  if(NOT compare_result EQUAL 0)
    _basilisk_dump_file("${TEST_NAME} stderr" "${compare_stderr}")
    message(FATAL_ERROR
      "${TEST_NAME}: log differs from ${TEST_REF_FILE}; "
      "actual log is ${compare_stderr}"
    )
  endif()
endif()
