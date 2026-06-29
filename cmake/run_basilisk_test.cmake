if(NOT DEFINED TEST_NAME)
  message(FATAL_ERROR "TEST_NAME is required")
endif()
if(NOT DEFINED TEST_SOURCE_DIR)
  message(FATAL_ERROR "TEST_SOURCE_DIR is required")
endif()
if(NOT DEFINED TEST_SOURCE_FILE)
  message(FATAL_ERROR "TEST_SOURCE_FILE is required")
endif()
if(NOT DEFINED TEST_STAGED_SOURCE)
  message(FATAL_ERROR "TEST_STAGED_SOURCE is required")
endif()
if(NOT DEFINED TEST_WORK_DIR)
  message(FATAL_ERROR "TEST_WORK_DIR is required")
endif()
if(NOT DEFINED TEST_QCC)
  message(FATAL_ERROR "TEST_QCC is required")
endif()
if(NOT DEFINED BASILISK_SOURCE_DIR)
  message(FATAL_ERROR "BASILISK_SOURCE_DIR is required")
endif()
if(NOT DEFINED TEST_TIMEOUT)
  set(TEST_TIMEOUT 10800)
endif()
if(NOT DEFINED TEST_CFLAGS)
  set(TEST_CFLAGS "")
endif()
if(NOT DEFINED TEST_LIBS)
  set(TEST_LIBS "")
endif()
if(NOT DEFINED TEST_OPENGLIBS)
  set(TEST_OPENGLIBS "")
endif()
if(NOT DEFINED TEST_CC)
  set(TEST_CC "")
endif()
if(NOT DEFINED TEST_EXECUTOR)
  set(TEST_EXECUTOR "")
endif()
if(NOT DEFINED TEST_MPI_RANKS)
  set(TEST_MPI_RANKS "")
endif()
if(NOT DEFINED TEST_STAGE_FILES)
  set(TEST_STAGE_FILES "")
endif()
# Lists passed through add_test() are encoded to avoid CMake treating them as
# extra command arguments.
string(REPLACE "|" ";" TEST_STAGE_FILES "${TEST_STAGE_FILES}")

# Print captured files only on failure, so normal CTest output stays compact.
function(_basilisk_dump_file label path)
  if(EXISTS "${path}")
    file(READ "${path}" contents)
    if(contents)
      message(STATUS "${label}:")
      message(STATUS "${contents}")
    endif()
  endif()
endfunction()

# Stage files with symlinks when possible. Copying is only a fallback for
# platforms or filesystems where symlink creation is unavailable.
function(_basilisk_stage_link source destination)
  if(EXISTS "${destination}" OR IS_SYMLINK "${destination}")
    return()
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "${source}" "${destination}"
    RESULT_VARIABLE link_result
  )
  if(NOT link_result EQUAL 0)
    if(IS_DIRECTORY "${source}")
      file(COPY "${source}" DESTINATION "${TEST_WORK_DIR}")
    else()
      file(COPY "${source}" DESTINATION "${TEST_WORK_DIR}")
    endif()
  endif()
endfunction()

# Each test runs in its own build-tree directory, mirroring the legacy
# src/runtest layout without writing generated files into the source tree.
file(REMOVE_RECURSE "${TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${TEST_WORK_DIR}" "${TEST_WORK_DIR}/bin")

if(NOT EXISTS "${TEST_SOURCE_FILE}")
  message(FATAL_ERROR "${TEST_NAME}: missing source file '${TEST_SOURCE_FILE}'")
endif()

_basilisk_stage_link("${TEST_SOURCE_FILE}" "${TEST_WORK_DIR}/${TEST_STAGED_SOURCE}")

# Most tests only need their source file. Runtime data is staged explicitly by
# basilisk_add_ctest(FILES ...).
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

set(test_exe "${TEST_WORK_DIR}/bin/${TEST_NAME}")
set(compile_stdout "${TEST_WORK_DIR}/compile.out")
set(compile_stderr "${TEST_WORK_DIR}/compile.err")
set(run_stdout "${TEST_WORK_DIR}/out")
set(run_stderr "${TEST_WORK_DIR}/log")
set(compare_stderr "${TEST_WORK_DIR}/log.compare")

separate_arguments(test_cflags UNIX_COMMAND "${TEST_CFLAGS}")
separate_arguments(test_libs UNIX_COMMAND "${TEST_LIBS}")
separate_arguments(test_executor UNIX_COMMAND "${TEST_EXECUTOR}")

# qcc resolves Basilisk includes/libs from these environment variables, and
# view.h expands OPENGLIBS through its autolink pragma.
set(test_env
  "BASILISK=${BASILISK_SOURCE_DIR}"
  "BASILISK_LIBDIR=${BASILISK_SOURCE_DIR}"
  "OPENGLIBS=${TEST_OPENGLIBS}"
)
if(TEST_CC)
  list(APPEND test_env "CC=${TEST_CC}" "CC99=${TEST_CC}")
endif()

# Compile the staged source with qcc. The executable lives under bin/ so any
# runtime files produced by the test stay in TEST_WORK_DIR.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_env}
    "${TEST_QCC}"
    -autolink
    -nolineno
    -disable-dimensions
    ${test_cflags}
    "-I${TEST_SOURCE_DIR}"
    -o "${test_exe}"
    "${TEST_STAGED_SOURCE}"
    ${test_libs}
    -lm
  WORKING_DIRECTORY "${TEST_WORK_DIR}"
  OUTPUT_FILE "${compile_stdout}"
  ERROR_FILE "${compile_stderr}"
  RESULT_VARIABLE compile_result
)

if(NOT compile_result EQUAL 0)
  _basilisk_dump_file("${TEST_NAME} compile stdout" "${compile_stdout}")
  _basilisk_dump_file("${TEST_NAME} compile stderr" "${compile_stderr}")
  message(FATAL_ERROR "${TEST_NAME}: qcc compilation failed")
endif()

# Execute exactly as the old harness does: stdout goes to out, stderr to log.
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_env} ${test_executor} "${test_exe}"
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

if(DEFINED TEST_REF_FILE AND NOT TEST_REF_FILE STREQUAL "")
  if(EXISTS "${TEST_REF_FILE}")
    set(compare_input "${run_stderr}")
    if(TEST_MPI_RANKS AND TEST_MPI_RANKS GREATER 1)
      set(merged_stderr "${TEST_WORK_DIR}/log.merged")
      file(READ "${run_stderr}" rank_log)
      file(WRITE "${merged_stderr}" "${rank_log}")
      math(EXPR last_rank "${TEST_MPI_RANKS} - 1")
      foreach(rank RANGE 1 ${last_rank})
        set(rank_stderr "${TEST_WORK_DIR}/log-${rank}")
        if(EXISTS "${rank_stderr}")
          file(READ "${rank_stderr}" rank_log)
          file(APPEND "${merged_stderr}" "${rank_log}")
        endif()
      endforeach()
      set(compare_input "${merged_stderr}")
    endif()

    file(READ "${compare_input}" compare_log)
    # ImageMagick 7 warns about the legacy convert command used by some tests.
    # That toolchain warning is not part of the numerical reference output.
    string(REGEX REPLACE
      "\n?WARNING: The convert command is deprecated in IMv7, use \"magick\" instead of \"convert\" or \"magick convert\"\n\n?"
      "\n"
      compare_log
      "${compare_log}"
    )
    file(WRITE "${compare_stderr}" "${compare_log}")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${compare_stderr}" "${TEST_REF_FILE}"
      RESULT_VARIABLE compare_result
    )
    if(NOT compare_result EQUAL 0)
      _basilisk_dump_file("${TEST_NAME} stdout" "${run_stdout}")
      _basilisk_dump_file("${TEST_NAME} stderr" "${compare_stderr}")
      message(FATAL_ERROR
        "${TEST_NAME}: log differs from ${TEST_REF_FILE}; "
        "actual log is ${compare_stderr}"
      )
    endif()
  endif()
endif()
