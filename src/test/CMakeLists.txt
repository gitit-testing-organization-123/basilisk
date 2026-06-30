include(${PROJECT_SOURCE_DIR}/cmake/basilisk_add_executable.cmake)

set(BASILISK_TEST_TIMEOUT 600 CACHE STRING "Default timeout, in seconds, for each Basilisk CTest test")
set(BASILISK_TEST_RUNNER "${PROJECT_SOURCE_DIR}/cmake/run_basilisk_executable_test.cmake")
set(BASILISK_DIMENSIONS_TEST_RUNNER "${PROJECT_SOURCE_DIR}/cmake/run_basilisk_dimensions_test.cmake")
set(BASILISK_TEST_OPENGLIBS
  "-lfb_tiny -L$<TARGET_FILE_DIR:tinyrenderer> -ltinyrenderer"
  CACHE STRING "OpenGL libraries used by qcc autolink while running Basilisk CTest tests")
set(BASILISK_TEST_LINKDIR
  "$<TARGET_FILE_DIR:glutils>"
  CACHE STRING "Link directory used by qcc autolink while running Basilisk CTest tests")
set(BASILISK_TEST_DEFAULT_DEFINES MTRACE=3)
set(BASILISK_VIEW_LIBS fb_tiny)

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

  # Set the default numproc for mpi tests to 2 if not given
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
  string(REPLACE ";" "|" stage_files_arg "${BCT_FILES}")
  string(REPLACE ";" "|" stage_test_files_arg "${BCT_FILES_FROM_TEST}")
  set(test_helper_paths
    "${PROJECT_BINARY_DIR}/src/kdt"
  )
  string(REPLACE ";" "|" test_helper_paths_arg "${test_helper_paths}")

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
      "-DTEST_STAGE_FILES=${stage_files_arg}"
      "-DTEST_STAGE_TEST_FILES=${stage_test_files_arg}"
      "-DTEST_EXTRA_PATH=${test_helper_paths_arg}"
      -P "${BASILISK_TEST_RUNNER}"
  )

  set(test_labels "basilisk")

  # Add mpi label if boolean flag MPI used 
  if(BCT_MPI)
    list(APPEND test_labels "mpi")
  endif()
  
  # Add openmp label if boolean flag OPENMP used 
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
      "-DTEST_BASILISK_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
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


# Default-pattern tests from the legacy Makefile.defs inventory that are already
# being carried explicitly in this CMake port.
basilisk_add_ctest(adaptbug LABELS short)
basilisk_add_ctest(adaptive LABELS short)
basilisk_add_ctest(advection LABELS short)
basilisk_add_ctest(artery1D LABELS short) 
basilisk_add_ctest(axi_rising_bubble LIBS ${BASILISK_VIEW_LIBS} LABELS verylong)
basilisk_add_ctest(bar LABELS long)
basilisk_add_ctest(bc0 LABELS short)

# Basic/default tests named by the legacy Makefile.
basilisk_add_ctest(halo LABELS short)
basilisk_add_ctest(interpolate LABELS short)
basilisk_add_ctest(events LABELS short )
basilisk_add_ctest(faces LABELS short)

# basilisk_add_ctest(coarsen QCC_FLAGS -progress LABELS short)
# basilisk_add_ctest(lid-restore SOURCE lid.c REF lid-restore.ref FILES lid-restore.dump) # needs lid.tst-produced dump staging
# basilisk_add_ctest(poiseuille.vtst SOURCE poiseuille.c REF poiseuille.ref) # legacy valgrind vtst mode

basilisk_add_ctest(tag.ctst SOURCE tag.c REF tag.cref QCC_FLAGS -grid=multigrid LABELS short)

# Explicit tests and aliases from the legacy Makefile.
# basilisk_add_ctest(basilisk DEFINES DEBUG=1 LIBS ${BASILISK_VIEW_LIBS} FILES basilisk.gnu) # needs generated basilisk.gnu

basilisk_add_ctest(bleck LIBS ${BASILISK_VIEW_LIBS} LABELS long)
basilisk_add_ctest(bleck.ctst SOURCE bleck.c REF bleck.cref QCC_FLAGS -grid=multigrid LIBS ${BASILISK_VIEW_LIBS} LABELS long)
basilisk_add_ctest(boundaries3 LABELS short)
basilisk_add_ctest(boundaries3.ctst SOURCE boundaries3.c REF boundaries3.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(boundary_face LABELS short)
basilisk_add_ctest(boundary_face.ctst SOURCE boundary_face.c REF boundary_face.ref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(boundary_face_cartesian SOURCE boundary_face.c REF boundary_face.ref QCC_FLAGS -grid=cartesian LABELS short)
basilisk_add_ctest(bubble-spherical SOURCE bubble.c REF bubble-spherical.ref DEFINES SPHERICAL=1 QCC_FLAGS -grid=multigrid1D LABELS short)
basilisk_add_ctest(collapse LABELS short)
basilisk_add_ctest(collapse-inviscid SOURCE collapse.c REF collapse-inviscid.ref DEFINES INVISCID=1 LABELS long)
basilisk_add_ctest(collapse-spherical SOURCE collapse.c REF collapse-spherical.ref DEFINES SPHERICAL=1 QCC_FLAGS -grid=multigrid1D LABELS short)
basilisk_add_ctest(dry LABELS short)
basilisk_add_ctest(dry-explicit SOURCE dry.c REF dry-explicit.ref DEFINES EXPLICIT=1 LABELS short)
basilisk_add_ctest(drybump LABELS short)
basilisk_add_ctest(drybump-explicit SOURCE drybump.c REF drybump-explicit.ref DEFINES EXPLICIT=1 LABELS short)
basilisk_add_ctest(drybump2D LABELS short)
basilisk_add_ctest(drybump2D-implicit SOURCE drybump2D.c REF drybump2D-implicit.ref DEFINES IMPLICIT=1 LABELS short)
basilisk_add_ctest(lake-tr LABELS short)
basilisk_add_ctest(lake-tr-explicit SOURCE lake-tr.c REF lake-tr-explicit.ref DEFINES EXPLICIT=1 LABELS short)
basilisk_add_ctest(lake-tr-ml SOURCE lake-tr.c REF lake-tr-ml.ref DEFINES ML=1 LABELS short)
basilisk_add_ctest(multiriverinflow REF multiriverinflow.ref OPENMP LABELS verylong)
basilisk_add_ctest(multiriverinflow.ctst SOURCE multiriverinflow.c REF multiriverinflow.ref OPENMP QCC_FLAGS -grid=multigrid LABELS verylong)
basilisk_add_ctest(parabola LABELS short) 
basilisk_add_ctest(parabola-explicit SOURCE parabola.c REF parabola-explicit.ref DEFINES EXPLICIT=1 LABELS short)
basilisk_add_ctest(parabola-ml SOURCE parabola.c REF parabola-ml.ref DEFINES ML=1 LABELS short)
basilisk_add_ctest(seawall LABELS short)
basilisk_add_ctest(seawallsv SOURCE seawall.c DEFINES SAINT_VENANT=1 LABELS short)
basilisk_add_ctest(seawall-ml SOURCE seawall.c REF seawall-ml.ref DEFINES ML=1 LABELS short)
basilisk_add_ctest(conical LABELS long)
basilisk_add_ctest(conicalsv SOURCE conical.c REF conicalsv.ref DEFINES SAINT_VENANT=1 LABELS short)
basilisk_add_ctest(conical-implicit SOURCE conical.c REF conical-implicit.ref DEFINES SAINT_VENANT=1 IMPLICIT=1 LABELS long)
basilisk_add_ctest(conical-ml SOURCE conical.c REF conical-ml.ref DEFINES ML=1 LABELS long)

# basilisk_add_ctest(curvature) # needs http://www.ida.upmc.fr/~zaleski/paris/Vofi-1.0.tar.gz
# basilisk_add_ctest(curvature.3D SOURCE curvature.c REF curvature.3D.ref QCC_FLAGS -grid=octree) # same

basilisk_add_ctest(implicit LABELS short)
basilisk_add_ctest(explicit SOURCE implicit.c REF explicit.ref DEFINES EXPLICIT=1 LABELS short)
basilisk_add_ctest(explicit-ml SOURCE implicit.c REF explicit-ml.ref DEFINES ML=1 EXPLICIT=1 LABELS short)
basilisk_add_ctest(implicit-ml SOURCE implicit.c REF implicit-ml.ref DEFINES ML=1 LABELS short)
basilisk_add_ctest(bore LABELS short)
basilisk_add_ctest(bore1 LABELS short)
basilisk_add_ctest(explosion LABELS short)
basilisk_add_ctest(explosion.ctst SOURCE explosion.c REF explosion.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(explosion.3D SOURCE explosion.c REF explosion.3D.ref QCC_FLAGS -grid=octree LABELS long)
basilisk_add_ctest(explosion3D SOURCE explosion.c REF explosion3D.ref QCC_FLAGS -grid=multigrid3D LABELS short)
basilisk_add_ctest(laplacian LABELS short)
basilisk_add_ctest(laplacian.ctst SOURCE laplacian.c REF laplacian.ref QCC_FLAGS -grid=multigrid LABELS short) 
basilisk_add_ctest(lid LABELS short)
basilisk_add_ctest(lidmac SOURCE lid.c REF lidmac.ref DEFINES MAC=1 LABELS short) 
basilisk_add_ctest(oscillation LABELS long)
basilisk_add_ctest(oscillation-compressible SOURCE oscillation.c REF oscillation-compressible.ref DEFINES COMPRESSIBLE=1 LABELS verylong)
basilisk_add_ctest(oscillation-momentum SOURCE oscillation.c REF oscillation-momentum.ref DEFINES MOMENTUM=1 LABELS ;png)
basilisk_add_ctest(ponds LIBS kdt LABELS short)
basilisk_add_ctest(ponds-implicit SOURCE ponds.c LIBS kdt REF ponds-implicit.ref DEFINES IMPLICIT=1 LABELS short)
basilisk_add_ctest(ponds-ml SOURCE ponds.c LIBS kdt REF ponds-ml.ref DEFINES ML=1 LABELS long)
basilisk_add_ctest(poiseuille-oldroydb LABELS long)
basilisk_add_ctest(poiseuille-fenep SOURCE poiseuille-oldroydb.c REF poiseuille-fenep.ref DEFINES FENE_P=1 LABELS long)
basilisk_add_ctest(poisson LABELS short)
basilisk_add_ctest(poisson.ctst SOURCE poisson.c REF poisson.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(rising LABELS long)
basilisk_add_ctest(rising-axi SOURCE rising.c REF rising-axi.ref DEFINES AXIS=1 LABELS long)
basilisk_add_ctest(rising-axi-momentum SOURCE rising.c REF rising-axi-momentum.ref DEFINES AXIS=1 MOMENTUM=1 LABELS long)
basilisk_add_ctest(rising-axi-clsvof SOURCE rising.c REF rising-axi-clsvof.ref DEFINES AXIS=1 LEVELSET=1 CLSVOF=1 LABELS long)
basilisk_add_ctest(rising2 SOURCE rising.c REF rising2.ref DEFINES CASE2=1 LABELS long)
basilisk_add_ctest(rising2-levelset SOURCE rising.c REF rising2-levelset.ref DEFINES CASE2=1 LEVELSET=1 LABELS long)
basilisk_add_ctest(rising2-clsvof SOURCE rising.c REF rising2-clsvof.ref DEFINES CASE2=1 LEVELSET=1 CLSVOF=1 LABELS long)
basilisk_add_ctest(rising-reduced SOURCE rising.c REF rising-reduced.ref DEFINES REDUCED=1 LABELS long)
basilisk_add_ctest(rising-levelset SOURCE rising.c REF rising-levelset.ref DEFINES LEVELSET=1 LABELS long)
basilisk_add_ctest(rising-clsvof SOURCE rising.c REF rising-clsvof.ref DEFINES LEVELSET=1 CLSVOF=1 LABELS long)
basilisk_add_ctest(reversed LABELS short)
basilisk_add_ctest(reversed.ctst SOURCE reversed.c REF reversed.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(rotate LABELS short)
basilisk_add_ctest(rotate.ctst SOURCE rotate.c REF rotate.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(rt LABELS short)
basilisk_add_ctest(rt-reduced SOURCE rt.c REF rt-reduced.ref DEFINES REDUCED=1 LABELS short)
basilisk_add_ctest(taylor-green LABELS short)
basilisk_add_ctest(taylor-green-all-mach SOURCE taylor-green.c REF taylor-green-all-mach.ref DEFINES ALL_MACH=1 LABELS short)

basilisk_add_ctest(view MPI MPI_RANKS 4 LIBS ${BASILISK_VIEW_LIBS})
basilisk_add_ctest(view.3D SOURCE view.c REF view.3D.ref MPI MPI_RANKS 4 QCC_FLAGS -grid=octree LIBS ${BASILISK_VIEW_LIBS})

basilisk_add_ctest(facefraction LABELS short)
basilisk_add_ctest(facefraction3D SOURCE facefraction.c REF facefraction3D.ref QCC_FLAGS -grid=multigrid3D LABELS long)

# Axisymmetric group.
basilisk_add_ctest(axiadvection MPI MPI_RANKS 7 DEFINES JACOBI=1)
basilisk_add_ctest(axi LABELS short)
basilisk_add_ctest(poiseuille-axi LABELS short)

# 3D group.
basilisk_add_ctest(circle.3D SOURCE circle.c REF circle.3D.ref QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(hf.3D SOURCE hf.c REF hf.3D.ref QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(periodic.3D SOURCE periodic.c REF periodic.3D.ref QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(poisson.3D SOURCE poisson.c REF poisson.3D.ref QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(refineu.3D SOURCE refineu.c REF refineu.3D.ref QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(solenoidal.3D SOURCE solenoidal.c REF solenoidal.3D.ref QCC_FLAGS -grid=octree LABELS short)

# Curvature group.
basilisk_add_ctest(hf LABELS short)
basilisk_add_ctest(hf3D LABELS short)
basilisk_add_ctest(hf.ctst SOURCE hf.c REF hf.cref QCC_FLAGS -grid=multigrid LABELS short)
basilisk_add_ctest(hf1 MPI MPI_RANKS 7) 
basilisk_add_ctest(hf-mask DEFINES TRASH=1 LABELS short)

# Multilayer group.
basilisk_add_ctest(dispersion-gn SOURCE dispersion.c REF dispersion-gn.ref LABELS short multilayer)
basilisk_add_ctest(dispersion DEFINES ML=1 LABELS long multilayer)
basilisk_add_ctest(beach-ml SOURCE beach.c REF beach-ml.ref DEFINES ML=1 LABELS short multilayer)
basilisk_add_ctest(bar-ml SOURCE bar.c REF bar-ml.ref DEFINES ML=1 LABELS long multilayer)
basilisk_add_ctest(lonlat-ml SOURCE lonlat.c REF lonlat-ml.ref DEFINES ML=1 LABELS short multilayer)
basilisk_add_ctest(wind-driven-nh SOURCE wind-driven.c REF wind-driven-nh.ref DEFINES ML=1 NH=1 LABELS short multilayer)
basilisk_add_ctest(wind-driven DEFINES ML=1 LABELS short multilayer)
basilisk_add_ctest(wind-driven-stvt SOURCE wind-driven.c REF wind-driven-stvt.ref LABELS short multilayer)
basilisk_add_ctest(large LABELS short multilayer)
basilisk_add_ctest(large-ns LABELS short multilayer)
basilisk_add_ctest(gaussian-stvt SOURCE gaussian.c REF gaussian-stvt.ref LABELS long multilayer)
basilisk_add_ctest(gaussian-hydro SOURCE gaussian.c REF gaussian-hydro.ref DEFINES ML=1 HYDRO=1 LABELS long multilayer)
basilisk_add_ctest(gaussian-nometric SOURCE gaussian.c REF gaussian-nometric.ref DEFINES ML=1 NOMETRIC=1 LABELS long multilayer)
basilisk_add_ctest(gaussian DEFINES ML=1 LABELS long multilayer)
basilisk_add_ctest(stokes LABELS long multilayer)
basilisk_add_ctest(stokes-ns LABELS long multilayer)
basilisk_add_ctest(galilean_invariance LABELS short multilayer)
basilisk_add_ctest(stommel-ml LABELS long multilayer)
basilisk_add_ctest(geo LABELS short multilayer)

# basilisk_add_ctest(nonlinear INCLUDES ${GSL_INCLUDE_DIRS} LIBS GSL::gsl)
# basilisk_add_ctest(nonlinear-ml SOURCE nonlinear.c REF nonlinear-ml.ref DEFINES ML=1 INCLUDES ${GSL_INCLUDE_DIRS} LIBS GSL::gsl)

basilisk_add_ctest(tsunami LABELS long multilayer)

# Multilayer stratified group.
basilisk_add_ctest(horn LABELS long mulilayer-stratified)
basilisk_add_ctest(lock LABELS short mulilayer-stratified) 
basilisk_add_ctest(overflow LABELS long mulilayer-stratified)
basilisk_add_ctest(overflow-isopycnal SOURCE overflow.c REF overflow-isopycnal.ref DEFINES ISOPYCNAL=1 LABELS short mulilayer-stratified)

# These tests are gnuplot-version dependent...
# basilisk_add_ctest(kh FILES kh.plot LABELS long mulilayer-stratified)
# basilisk_add_ctest(kh-ns MPI MPI_RANKS 8 FILES kh-ns.plot LABELS long mulilayer-stratified)
# basilisk_add_ctest(kh-hydro SOURCE kh.c REF kh-hydro.ref DEFINES HYDRO=1 FILES kh-hydro.plot LABELS long mulilayer-stratified)

# MPI and parallel-output groups
basilisk_add_ctest(indexing REF indexing.ref MPI MPI_RANKS 3 LABELS short)
basilisk_add_ctest(indexing.3D SOURCE indexing.c REF indexing.3D.ref MPI MPI_RANKS 3 QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(mpi-restriction MPI MPI_RANKS 3 LABELS short)
basilisk_add_ctest(mpi-restriction.3D SOURCE mpi-restriction.c MPI MPI_RANKS 3 QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(mpi-reduce MPI MPI_RANKS 3 LABELS short)
basilisk_add_ctest(mpi-refine MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(mpi-refine1 MPI MPI_RANKS 11 LABELS short)
basilisk_add_ctest(mpi-refine.3D SOURCE mpi-refine.c MPI MPI_RANKS 4 QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(mpi-laplacian MPI MPI_RANKS 3 LABELS short)
basilisk_add_ctest(mpi-laplacian.3D SOURCE mpi-laplacian.c REF mpi-laplacian.ref MPI MPI_RANKS 3 QCC_FLAGS -grid=octree LABELS short)
basilisk_add_ctest(mpi-circle MPI MPI_RANKS 5 LABELS short)
basilisk_add_ctest(mpi-circle1 SOURCE mpi-circle.c REF mpi-circle1.ref MPI MPI_RANKS 6 LABELS short)
basilisk_add_ctest(mpi-flux MPI MPI_RANKS 6 LABELS short)
basilisk_add_ctest(mpi-interpu MPI MPI_RANKS 5 LABELS short)
basilisk_add_ctest(mpi-coarsen MPI MPI_RANKS 2 LABELS short)
basilisk_add_ctest(mpi-coarsen1 MPI MPI_RANKS 5 LABELS short)
basilisk_add_ctest(pdump MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(restore MPI MPI_RANKS 7 DEPENDS pdump FILES_FROM_TEST pdump:restore.dump LABELS short)
basilisk_add_ctest(pdump-multigrid SOURCE pdump.c MPI MPI_RANKS 16 QCC_FLAGS -grid=multigrid LABELS long)
basilisk_add_ctest(restore-multigrid MPI MPI_RANKS 16 QCC_FLAGS -grid=multigrid DEPENDS pdump-multigrid FILES_FROM_TEST pdump-multigrid:restore.dump=restore-multigrid.dump LABELS short)
basilisk_add_ctest(restore-tree MPI MPI_RANKS 23 DEFINES DEBUGCOND=false DEPENDS pdump FILES_FROM_TEST pdump:restore.dump=restore-tree.dump LABELS short)
basilisk_add_ctest(restore-multigrid-serial DEPENDS pdump-multigrid FILES_FROM_TEST pdump-multigrid:restore.dump=restore-multigrid-serial.dump LABELS short)
basilisk_add_ctest(dump-rectangle LABELS short)
basilisk_add_ctest(restore-rectangle DEPENDS dump-rectangle FILES_FROM_TEST dump-rectangle:restore-rectangle.dump LABELS short)
basilisk_add_ctest(bump2Dp MPI MPI_RANKS 55 LABELS short)
basilisk_add_ctest(bump2Dp-restore SOURCE bump2Dp.c REF bump2Dp-restore.ref MPI MPI_RANKS 15 DEPENDS bump2Dp FILES_FROM_TEST bump2Dp:dump=bump2Dp-restore.dump LABELS short)
basilisk_add_ctest(poiseuille-periodic MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(gfs MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(gfsi DEPENDS gfs FILES_FROM_TEST gfs:gfsi.gfs LABELS short)
basilisk_add_ctest(mpi-grid MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(mpi-periodic-3D SOURCE periodic.c REF mpi-periodic-3D.ref MPI MPI_RANKS 8 QCC_FLAGS -grid=multigrid3D LABELS short)
basilisk_add_ctest(source MPI MPI_RANKS 7 LABELS long)
basilisk_add_ctest(tag MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(tag1 MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(boundary_vertex MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(boundary_vertex3D MPI MPI_RANKS 7 LABELS short)
basilisk_add_ctest(foreach_bnd1 MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(vertices-bc MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(reductions MPI MPI_RANKS 4 LABELS short)
basilisk_add_ctest(balance5 MPI MPI_RANKS 9 LABELS short)
basilisk_add_ctest(balance6 SOURCE balance5.c REF balance5.ref MPI MPI_RANKS 17 LABELS short)
basilisk_add_ctest(balance7 MPI MPI_RANKS 17 LABELS short)
basilisk_add_ctest(openmp-reduce SOURCE mpi-reduce.c REF openmp-reduce.ref OPENMP LABELS short)

# Periodic boundary group.
basilisk_add_ctest(periodic1 QCC_FLAGS -grid=bitree DEFINES TRASH=1 LABELS short periodic)
basilisk_add_ctest(periodic2 SOURCE periodic1.c REF periodic2.ref DEFINES TRASH=1 LABELS short periodic)
# basilisk_add_ctest(periodic3 SOURCE periodic1.c QCC_FLAGS -grid=octree DEFINES TRASH=1) # no periodic3.ref in tree

# Embedded boundary group.
basilisk_add_ctest(neumann LIBS ${BASILISK_VIEW_LIBS} LABELS short embed)
basilisk_add_ctest(dirichlet SOURCE neumann.c REF dirichlet.ref DEFINES DIRICHLET=1 LIBS ${BASILISK_VIEW_LIBS} LABELS short embed)
basilisk_add_ctest(poiseuille45 LIBS ${BASILISK_VIEW_LIBS} LABELS short embed)
basilisk_add_ctest(couette LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(wannier LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(hydrostatic2 MPI MPI_RANKS 8 LIBS ${BASILISK_VIEW_LIBS} LABELS embed) 
basilisk_add_ctest(cylinders LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(porous LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(porous1 LABELS long embed)
basilisk_add_ctest(uf LABELS short embed)
basilisk_add_ctest(starting LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(neumann3D LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(dirichlet3D SOURCE neumann3D.c REF dirichlet3D.ref DEFINES DIRICHLET=1 LIBS ${BASILISK_VIEW_LIBS} LABELS long embed)
basilisk_add_ctest(hydrostatic3 LIBS ${BASILISK_VIEW_LIBS} LABELS short embed)
basilisk_add_ctest(spheres LABELS long embed)
basilisk_add_ctest(neumann-axi LIBS ${BASILISK_VIEW_LIBS} LABELS short embed) 
basilisk_add_ctest(dirichlet-axi SOURCE neumann-axi.c REF dirichlet-axi.ref DEFINES DIRICHLET=1 LIBS ${BASILISK_VIEW_LIBS} LABELS short embed)

# GOTM and optional external-model group.
# basilisk_add_ctest(couette-gotm LABELS gotm) # needs GOTM/CVMix runtime parity check
# basilisk_add_ctest(entrainment LABELS gotm) # needs GOTM/CVMix runtime parity check
# basilisk_add_ctest(ows_papa LABELS gotm) # needs GOTM/CVMix runtime parity check
# basilisk_add_ctest(ows_papa-kpp SOURCE ows_papa.c REF ows_papa-kpp.ref DEFINES KPP=1 LABELS gotm) # needs GOTM/CVMix runtime parity check

# Miscellaneous explicit groups.
basilisk_add_ctest(stencils DEFINES PRINTBOUNDARY=1 LABELS short)
basilisk_add_ctest(capwave-clsvof SOURCE capwave.c REF capwave-clsvof.ref DEFINES CLSVOF=1 LABELS long)

# Dimensions-only checks from the legacy Makefile.
basilisk_add_ctest_dimensions(bump2D LABELS short)
basilisk_add_ctest_dimensions(capwave FILES prosperetti.h LABELS short)
basilisk_add_ctest_dimensions(lid LABELS short)
basilisk_add_ctest_dimensions(rising-axi SOURCE rising.c DEFINES AXIS=1 LABELS short)
basilisk_add_ctest_dimensions(rising-reduced SOURCE rising.c DEFINES REDUCED=1 LABELS short)
basilisk_add_ctest_dimensions(vortex DEFINES JACOBI=1 LABELS short)
basilisk_add_ctest_dimensions(sessile LABELS short)
basilisk_add_ctest_dimensions(large LABELS short)
basilisk_add_ctest_dimensions(lock LABELS short)
basilisk_add_ctest_dimensions(wind-driven-stvt SOURCE wind-driven.c LABELS short)
basilisk_add_ctest_dimensions(shock LABELS short)
basilisk_add_ctest_dimensions(starting LABELS short)
basilisk_add_ctest_dimensions(layered LABELS short)
basilisk_add_ctest_dimensions(stokes FILES stokes.h LABELS short)
basilisk_add_ctest_dimensions(stokes-ns FILES stokes.h LABELS short)
basilisk_add_ctest_dimensions(lonlat LABELS short)
basilisk_add_ctest_dimensions(overflow LABELS short)
basilisk_add_ctest_dimensions(bleck LABELS short)

# GPU/CUDA/HIP groups. GPU backends are opt-in because they need backend-specific runtime support.
if(BASILISK_ENABLE_GLSL_TESTS)
  if(NOT BASILISK_USE_GLSL)
    message(FATAL_ERROR
      "BASILISK_ENABLE_GLSL_TESTS requires BASILISK_USE_GLSL=ON")
  endif()

  set(BASILISK_GLSL_QCC_FLAGS -grid=gpu/multigrid)
  set(BASILISK_GLSL_DEFINES PRINTNSHADERS)
  set(BASILISK_GLSL_LIBS basilisk::gpu)

  basilisk_add_ctest(gpu.gpu SOURCE gpu.c REF gpu.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(gpu-cpu.gpu SOURCE gpu.c REF gpu-cpu.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS} -cpu
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(gpu-reduce.gpu SOURCE gpu-reduce.c REF gpu-reduce.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(gpu-boundary.gpu SOURCE gpu-boundary.c REF gpu-boundary.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(restriction1.gpu SOURCE restriction1.c REF restriction1.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(layers.gpu SOURCE layers.c REF layers.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  # basilisk_add_ctest(nonlinear.gpu SOURCE nonlinear.c REF nonlinear.gpu.ref QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS} DEFINES ${BASILISK_GLSL_DEFINES} LIBS ${BASILISK_GLSL_LIBS} LABELS short gpu glsl) # needs GSL
  basilisk_add_ctest(advection.gpu SOURCE advection.c REF advection.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(bump2D-gpu.gpu SOURCE bump2D-gpu.c REF bump2D-gpu.ref
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(reversed.gpu SOURCE reversed.c REF reversed.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(axi.gpu SOURCE axi.c REF axi.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(lid.gpu SOURCE lid.c REF lid.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(stommel-ml.gpu SOURCE stommel-ml.c REF stommel-ml.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(bleck.gpu SOURCE bleck.c REF bleck.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES}
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
  basilisk_add_ctest(rising-clsvof.gpu SOURCE rising.c REF rising-clsvof.gpu.ref
    QCC_FLAGS ${BASILISK_GLSL_QCC_FLAGS}
    DEFINES ${BASILISK_GLSL_DEFINES} LEVELSET=1 CLSVOF=1
    LIBS ${BASILISK_GLSL_LIBS}
    LABELS short gpu glsl)
endif()
# basilisk_add_ctest(gpu.cuda SOURCE gpu.c REF gpu.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(gpu-reduce.cuda SOURCE gpu-reduce.c REF gpu-reduce.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(restriction1.cuda SOURCE restriction1.c REF restriction1.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(gpu-boundary.cuda SOURCE gpu-boundary.c REF gpu-boundary.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(bump2D-gpu.cuda SOURCE bump2D-gpu.c REF bump2D-gpu.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(advection.cuda SOURCE advection.c REF advection.cuda.ref QCC_FLAGS -grid=cuda/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(gpu.hip SOURCE gpu.c REF gpu.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(gpu-reduce.hip SOURCE gpu-reduce.c REF gpu-reduce.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(restriction1.hip SOURCE restriction1.c REF restriction1.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(gpu-boundary.hip SOURCE gpu-boundary.c REF gpu-boundary.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(bump2D-gpu.hip SOURCE bump2D-gpu.c REF bump2D-gpu.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)
# basilisk_add_ctest(advection.hip SOURCE advection.c REF advection.hip.ref QCC_FLAGS -grid=hip/multigrid DEFINES PRINTNSHADERS)

# CADNA group. Left disabled until CADNA compiler/link flags are modeled.
# basilisk_add_ctest(poiseuille.CADNA SOURCE poiseuille.c REF poiseuille.CADNA.ref QCC_FLAGS -cadna LABELS cadna)
# basilisk_add_ctest(sag.CADNA SOURCE sag.c REF sag.CADNA.ref QCC_FLAGS -cadna LABELS cadna)
# basilisk_add_ctest(poisson.CADNA SOURCE poisson.c REF poisson.CADNA.ref QCC_FLAGS -cadna LABELS cadna)
