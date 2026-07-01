if(NOT COMMAND add_basilisk_executable)
  include("${CMAKE_CURRENT_LIST_DIR}/basilisk_add_executable.cmake")
endif()

set(BASILISK_TEST_TIMEOUT 3600 CACHE STRING
  "Default timeout, in seconds, for each Basilisk CTest test")

if(NOT DEFINED BASILISK_TEST_RUNNER)
  set(BASILISK_TEST_RUNNER
    "${CMAKE_CURRENT_LIST_DIR}/run_basilisk_executable_test.cmake")
endif()

if(NOT DEFINED BASILISK_DIMENSIONS_TEST_RUNNER)
  set(BASILISK_DIMENSIONS_TEST_RUNNER
    "${CMAKE_CURRENT_LIST_DIR}/run_basilisk_dimensions_test.cmake")
endif()

if(NOT DEFINED BASILISK_TEST_BASILISK_DIR)
  if(DEFINED BASILISK_INCLUDE_DIR)
    set(BASILISK_TEST_BASILISK_DIR "${BASILISK_INCLUDE_DIR}")
  elseif(EXISTS "${PROJECT_SOURCE_DIR}/src")
    set(BASILISK_TEST_BASILISK_DIR "${PROJECT_SOURCE_DIR}/src")
  endif()
endif()

if(NOT DEFINED BASILISK_TEST_BASILISK_LIBDIR)
  set(BASILISK_TEST_BASILISK_LIBDIR "${BASILISK_TEST_BASILISK_DIR}")
endif()

if(NOT DEFINED BASILISK_TEST_BASILISK_INCLUDE_DIR)
  if(DEFINED BASILISK_INCLUDE_DIR)
    set(BASILISK_TEST_BASILISK_INCLUDE_DIR "${BASILISK_INCLUDE_DIR}")
  elseif(EXISTS "${PROJECT_SOURCE_DIR}/src")
    set(BASILISK_TEST_BASILISK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}")
  endif()
endif()

if(NOT DEFINED BASILISK_TEST_LINKDIR)
  if(TARGET glutils)
    set(BASILISK_TEST_LINKDIR "$<TARGET_FILE_DIR:glutils>")
  elseif(DEFINED BASILISK_LIBDIR)
    set(BASILISK_TEST_LINKDIR "${BASILISK_LIBDIR}")
  else()
    set(BASILISK_TEST_LINKDIR "")
  endif()
endif()

if(NOT DEFINED BASILISK_TEST_OPENGLIBS)
  if(TARGET tinyrenderer)
    set(BASILISK_TEST_OPENGLIBS
      "-lfb_tiny -L$<TARGET_FILE_DIR:tinyrenderer> -ltinyrenderer")
  else()
    set(BASILISK_TEST_OPENGLIBS "-lfb_tiny -ltinyrenderer")
  endif()
endif()

if(NOT DEFINED BASILISK_TEST_EXTRA_PATH)
  set(BASILISK_TEST_EXTRA_PATH)
  if(DEFINED BASILISK_BINDIR)
    list(APPEND BASILISK_TEST_EXTRA_PATH "${BASILISK_BINDIR}")
  endif()
  if(EXISTS "${PROJECT_SOURCE_DIR}/src")
    list(APPEND BASILISK_TEST_EXTRA_PATH "${PROJECT_SOURCE_DIR}/src")
    list(APPEND BASILISK_TEST_EXTRA_PATH "${PROJECT_BINARY_DIR}/src/kdt")
  endif()
endif()

if(NOT DEFINED BASILISK_TEST_DEFAULT_DEFINES)
  set(BASILISK_TEST_DEFAULT_DEFINES)
endif()

if(NOT DEFINED BASILISK_VIEW_LIBS)
  if(TARGET fb_tiny)
    set(BASILISK_VIEW_LIBS fb_tiny)
  elseif(TARGET basilisk::fb_tiny)
    set(BASILISK_VIEW_LIBS basilisk::fb_tiny)
  else()
    set(BASILISK_VIEW_LIBS)
  endif()
endif()

function(_basilisk_abs_path out path base_dir)
  if(IS_ABSOLUTE "${path}")
    set(result "${path}")
  else()
    set(result "${base_dir}/${path}")
  endif()
  set("${out}" "${result}" PARENT_SCOPE)
endfunction()

function(basilisk_add_ctest NAME)
  set(options MPI OPENMP)
  set(one_value_args SOURCE REF TIMEOUT MPI_RANKS)
  set(multi_value_args
    ALIASES
    DEFINES
    DEPENDS
    EXECUTOR
    FILES
    FILES_FROM_TEST
    INCLUDES
    LABELS
    LIBS
    QCC_FLAGS
  )

  cmake_parse_arguments(BCT
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
  )

  if(NOT BCT_SOURCE)
    set(BCT_SOURCE "${NAME}.c")
  endif()
  _basilisk_abs_path(source_file "${BCT_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}")

  if(BCT_REF)
    set(ref_file "${BCT_REF}")
  else()
    set(ref_file "${NAME}.ref")
  endif()
  _basilisk_abs_path(ref_file "${ref_file}" "${CMAKE_CURRENT_SOURCE_DIR}")

  if(BCT_TIMEOUT)
    set(test_timeout "${BCT_TIMEOUT}")
  else()
    set(test_timeout "${BASILISK_TEST_TIMEOUT}")
  endif()

  set(basilisk_executable_args
    NAME "${NAME}"
    SOURCE "${source_file}"
    DEFINES ${BASILISK_TEST_DEFAULT_DEFINES} ${BCT_DEFINES}
    INCLUDES ${BCT_INCLUDES}
    LIBS ${BCT_LIBS}
    QCC_FLAGS ${BCT_QCC_FLAGS}
  )
  if(BCT_OPENMP)
    list(APPEND basilisk_executable_args OPENMP)
  endif()

  if(BCT_MPI)
    add_basilisk_mpi_executable(${basilisk_executable_args})
  else()
    add_basilisk_executable(${basilisk_executable_args})
  endif()

  set(test_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
  if(EXISTS "${test_work_dir}" AND NOT IS_DIRECTORY "${test_work_dir}")
    file(REMOVE "${test_work_dir}")
  endif()

  set_target_properties("${NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${test_work_dir}"
    BUILD_RPATH "${test_work_dir}"
  )

  if(BCT_MPI AND NOT BCT_MPI_RANKS)
    set(BCT_MPI_RANKS 2)
  endif()

  set(test_executor ${BCT_EXECUTOR})

  if(BCT_MPI AND NOT test_executor)
    find_program(BASILISK_MPIRUN mpirun)

    if(NOT BASILISK_MPIRUN)
      message(FATAL_ERROR "${NAME}: MPI test requires mpirun")
    endif()

    set(test_executor "${BASILISK_MPIRUN}" "--oversubscribe" "-np" "${BCT_MPI_RANKS}")
  endif()

  string(REPLACE ";" "|" test_executor_arg "${test_executor}")
  string(REPLACE ";" "|" aliases_arg "${BCT_ALIASES}")
  string(REPLACE ";" "|" stage_files_arg "${BCT_FILES}")
  string(REPLACE ";" "|" stage_test_files_arg "${BCT_FILES_FROM_TEST}")
  string(REPLACE ";" "|" test_helper_paths_arg "${BASILISK_TEST_EXTRA_PATH}")

  add_test(
    NAME "${NAME}"
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_NAME=${NAME}"
      "-DTEST_EXECUTABLE=$<TARGET_FILE:${NAME}>"
      "-DTEST_WORK_DIR=${test_work_dir}"
      "-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DTEST_BINARY_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DTEST_REF_FILE=${ref_file}"
      "-DTEST_TIMEOUT=${test_timeout}"
      "-DTEST_EXECUTOR=${test_executor_arg}"
      "-DTEST_MPI_RANKS=${BCT_MPI_RANKS}"
      "-DTEST_ALIASES=${aliases_arg}"
      "-DTEST_STAGE_FILES=${stage_files_arg}"
      "-DTEST_STAGE_TEST_FILES=${stage_test_files_arg}"
      "-DTEST_EXTRA_PATH=${test_helper_paths_arg}"
      -P "${BASILISK_TEST_RUNNER}"
  )

  set(test_labels "basilisk")

  if(BCT_MPI)
    list(APPEND test_labels "mpi")
  endif()

  if(BCT_OPENMP)
    list(APPEND test_labels "openmp")
  endif()
  list(APPEND test_labels ${BCT_LABELS})

  set_tests_properties("${NAME}" PROPERTIES
    LABELS "${test_labels}"
    TIMEOUT "${test_timeout}"
  )

  if(BCT_MPI)
    set_tests_properties("${NAME}" PROPERTIES
      PROCESSORS "${BCT_MPI_RANKS}"
    )
  endif()

  if(BCT_DEPENDS)
    set_tests_properties("${NAME}" PROPERTIES
      DEPENDS "${BCT_DEPENDS}"
    )
  endif()
endfunction()

function(basilisk_add_ctest_dimensions NAME)
  set(options)
  set(one_value_args SOURCE REF TIMEOUT)
  set(multi_value_args
    DEFINES
    FILES
    INCLUDES
    LABELS
    QCC_FLAGS
  )

  cmake_parse_arguments(BCD
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
  )

  if(NOT BCD_SOURCE)
    set(BCD_SOURCE "${NAME}.c")
  endif()
  _basilisk_abs_path(source_file "${BCD_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}")

  if(BCD_REF)
    set(ref_file "${BCD_REF}")
  else()
    set(ref_file "${NAME}.dims.ref")
  endif()
  _basilisk_abs_path(ref_file "${ref_file}" "${CMAKE_CURRENT_SOURCE_DIR}")

  if(BCD_TIMEOUT)
    set(test_timeout "${BCD_TIMEOUT}")
  else()
    set(test_timeout "${BASILISK_TEST_TIMEOUT}")
  endif()

  set(test_name "${NAME}.dims")
  set(test_work_dir "${CMAKE_CURRENT_BINARY_DIR}/${test_name}")

  string(REPLACE ";" "|" test_defines_arg "${BCD_DEFINES}")
  string(REPLACE ";" "|" test_stage_files_arg "${BCD_FILES}")
  string(REPLACE ";" "|" test_includes_arg "${BCD_INCLUDES}")
  string(REPLACE ";" "|" test_qcc_flags_arg "${BCD_QCC_FLAGS}")

  add_test(
    NAME "${test_name}"
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_NAME=${test_name}"
      "-DTEST_DIMENSIONS_NAME=${NAME}"
      "-DTEST_SOURCE_FILE=${source_file}"
      "-DTEST_WORK_DIR=${test_work_dir}"
      "-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DTEST_BASILISK_DIR=${BASILISK_TEST_BASILISK_DIR}"
      "-DTEST_BASILISK_LIBDIR=${BASILISK_TEST_BASILISK_LIBDIR}"
      "-DTEST_BASILISK_INCLUDE_DIR=${BASILISK_TEST_BASILISK_INCLUDE_DIR}"
      "-DTEST_QCC=$<TARGET_FILE:basilisk::qcc>"
      "-DTEST_REF_FILE=${ref_file}"
      "-DTEST_TIMEOUT=${test_timeout}"
      "-DTEST_DEFINES=${test_defines_arg}"
      "-DTEST_STAGE_FILES=${test_stage_files_arg}"
      "-DTEST_INCLUDES=${test_includes_arg}"
      "-DTEST_QCC_FLAGS=${test_qcc_flags_arg}"
      "-DTEST_BASILISK_LINKDIR=${BASILISK_TEST_LINKDIR}"
      "-DTEST_OPENGLIBS=${BASILISK_TEST_OPENGLIBS}"
      -P "${BASILISK_DIMENSIONS_TEST_RUNNER}"
  )

  set(test_labels "basilisk" "dimensions")
  list(APPEND test_labels ${BCD_LABELS})
  set_tests_properties("${test_name}" PROPERTIES
    LABELS "${test_labels}"
    TIMEOUT "${test_timeout}"
  )
endfunction()
