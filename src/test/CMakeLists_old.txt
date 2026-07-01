# Defaults used by the CTest wrapper around qcc. Individual tests can add
# CFLAGS/LIBS/FILES when their legacy Makefile rule needs them.
set(BASILISK_TEST_DEFAULT_CFLAGS "-O2 -DMTRACE=3" CACHE STRING "Default qcc flags for Basilisk CTest tests")
set(BASILISK_TEST_OPENGLIBS "-lfb_tiny -ltinyrenderer" CACHE STRING "OpenGL libraries used by qcc autolink while running Basilisk CTest tests")
set(BASILISK_TEST_TIMEOUT 10800 CACHE STRING "Default timeout, in seconds, for each Basilisk CTest test")
set(BASILISK_TEST_RUNNER "${PROJECT_SOURCE_DIR}/cmake/run_basilisk_test.cmake")

# Register one Basilisk source test. The runner compiles SOURCE with qcc,
# runs it in build/src/test/<name>/, and compares stderr with REF.
function(basilisk_add_ctest name)
  set(options CTST)
  set(one_value_args SOURCE REF CC EXECUTOR TIMEOUT MPI_RANKS)
  set(multi_value_args CFLAGS LIBS LABELS DEPENDS FILES)
  cmake_parse_arguments(BT "${options}" "${one_value_args}"
                        "${multi_value_args}" ${ARGN})

  if(BT_SOURCE)
    set(source_file "${BT_SOURCE}")
  else()
    set(source_file "${name}.c")
  endif()
  if(NOT IS_ABSOLUTE "${source_file}")
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
  endif()

  if(BT_REF)
    set(ref_file "${BT_REF}")
  elseif(BT_CTST)
    set(ref_file "${name}.cref")
  else()
    set(ref_file "${name}.ref")
  endif()
  if(ref_file AND NOT IS_ABSOLUTE "${ref_file}")
    set(ref_file "${CMAKE_CURRENT_SOURCE_DIR}/${ref_file}")
  endif()

  if(BT_TIMEOUT)
    set(test_timeout "${BT_TIMEOUT}")
  else()
    set(test_timeout "${BASILISK_TEST_TIMEOUT}")
  endif()

  # CTest command arguments are strings, so list-valued settings are encoded
  # before being decoded again in run_basilisk_test.cmake.
  set(test_cflags "${BASILISK_TEST_DEFAULT_CFLAGS}")
  if(BT_CTST)
    list(APPEND test_cflags "-grid=multigrid")
  endif()
  list(APPEND test_cflags ${BT_CFLAGS})
  string(REPLACE ";" " " test_cflags_arg "${test_cflags}")
  string(REPLACE ";" " " test_libs_arg "${BT_LIBS}")
  string(REPLACE ";" " " test_labels_arg "${BT_LABELS}")
  string(REPLACE ";" "|" test_stage_files_arg "${BT_FILES}")

  set(test_name "basilisk.${name}")
  add_test(
    NAME "${test_name}"
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_NAME=${name}"
      "-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DTEST_SOURCE_FILE=${source_file}"
      "-DTEST_STAGED_SOURCE=${name}.c"
      "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}"
      "-DTEST_QCC=$<TARGET_FILE:qcc>"
      "-DBASILISK_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src"
      "-DTEST_REF_FILE=${ref_file}"
      "-DTEST_CFLAGS=${test_cflags_arg}"
      "-DTEST_LIBS=${test_libs_arg}"
      "-DTEST_OPENGLIBS=${BASILISK_TEST_OPENGLIBS}"
      "-DTEST_STAGE_FILES=${test_stage_files_arg}"
      "-DTEST_CC=${BT_CC}"
      "-DTEST_EXECUTOR=${BT_EXECUTOR}"
      "-DTEST_MPI_RANKS=${BT_MPI_RANKS}"
      "-DTEST_TIMEOUT=${test_timeout}"
      -P "${BASILISK_TEST_RUNNER}"
  )

  set(labels "basilisk" "src-test")
  if(BT_CTST)
    list(APPEND labels "ctst" "multigrid")
  endif()
  list(APPEND labels ${BT_LABELS})

  set_tests_properties("${test_name}" PROPERTIES
    LABELS "${labels}"
    TIMEOUT "${test_timeout}"
  )

  if(BT_DEPENDS)
    set(test_depends)
    foreach(dependency IN LISTS BT_DEPENDS)
      list(APPEND test_depends "basilisk.${dependency}")
    endforeach()
    set_tests_properties("${test_name}" PROPERTIES DEPENDS "${test_depends}")
  endif()
endfunction()

# Tests listed here have non-default flags, MPI/GPU requirements, generated
# source aliases, or other special behavior registered explicitly below.
set(BASILISK_AUTO_TEST_EXCLUDES
  axiadvection
  balance5
  balance7
  basilisk
  boundary_vertex
  boundary_vertex3D
  bump2D-gpu
  bump2Dp
  coarsen
  dispersion
  foreach_bnd1
  gfs
  gfsi
  gaussian
  gpu
  gpu-boundary
  gpu-reduce
  hf-mask
  hf1
  hydrostatic2
  indexing
  kh-ns
  mpi-circle
  mpi-coarsen
  mpi-coarsen1
  mpi-flux
  mpi-grid
  mpi-interpu
  mpi-laplacian
  mpi-reduce
  mpi-refine
  mpi-refine1
  mpi-restriction
  multiriverinflow
  pdump
  periodic1
  poiseuille-periodic
  reductions
  restore
  restore-multigrid
  restore-multigrid-serial
  restore-rectangle
  restore-tree
  shockwave
  source
  tag
  tag1
  vertices-bc
  view
  vortex
  shrinking
  stencils
  wind-driven
)

# Simple serial tests are discovered automatically when <name>.c and
# <name>.ref exist and no special legacy Makefile rule is needed.
file(GLOB basilisk_test_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.c")
foreach(test_source IN LISTS basilisk_test_sources)
  get_filename_component(test_stem "${test_source}" NAME_WE)
  list(FIND BASILISK_AUTO_TEST_EXCLUDES "${test_stem}" excluded_index)
  if(excluded_index EQUAL -1 AND
     EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_stem}.ref")
    basilisk_add_ctest("${test_stem}" SOURCE "${test_source}" LABELS serial)
  endif()
endforeach()

# Variants encoded in the legacy test Makefile.
basilisk_add_ctest("bleck.ctst" SOURCE "bleck.c" REF "bleck.cref" CTST LABELS serial)
basilisk_add_ctest("boundaries3.ctst" SOURCE "boundaries3.c" REF "boundaries3.cref" CTST LABELS serial)
basilisk_add_ctest("capwave.ctst" SOURCE "capwave.c" REF "capwave.cref" CTST LABELS serial)
basilisk_add_ctest("explosion.ctst" SOURCE "explosion.c" REF "explosion.cref" CTST LABELS serial)
basilisk_add_ctest("hf.ctst" SOURCE "hf.c" REF "hf.cref" CTST LABELS serial)
basilisk_add_ctest("poisson.ctst" SOURCE "poisson.c" REF "poisson.cref" CTST LABELS serial)
basilisk_add_ctest("reversed.ctst" SOURCE "reversed.c" REF "reversed.cref" CTST LABELS serial)
basilisk_add_ctest("rotate.ctst" SOURCE "rotate.c" REF "rotate.cref" CTST LABELS serial)
basilisk_add_ctest("shockwave.ctst" SOURCE "shockwave.c" REF "shockwave.cref" CTST LABELS serial)
basilisk_add_ctest("tag.ctst" SOURCE "tag.c" REF "tag.cref" CTST LABELS serial)

basilisk_add_ctest("bubble-spherical" SOURCE "bubble.c" CFLAGS "-grid=multigrid1D" "-DSPHERICAL=1" LABELS serial)
basilisk_add_ctest("collapse-inviscid" SOURCE "collapse.c" CFLAGS "-DINVISCID=1" LABELS serial)
basilisk_add_ctest("collapse-spherical" SOURCE "collapse.c" CFLAGS "-grid=multigrid1D" "-DSPHERICAL=1" LABELS serial)
basilisk_add_ctest("dry-explicit" SOURCE "dry.c" CFLAGS "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("drybump-explicit" SOURCE "drybump.c" CFLAGS "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("drybump2D-implicit" SOURCE "drybump2D.c" CFLAGS "-DIMPLICIT=1" LABELS serial)
basilisk_add_ctest("lake-tr-explicit" SOURCE "lake-tr.c" CFLAGS "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("lake-tr-ml" SOURCE "lake-tr.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("multiriverinflow" SOURCE "multiriverinflow.c" CFLAGS "-fopenmp" LABELS serial openmp)
basilisk_add_ctest("openmp-reduce" SOURCE "mpi-reduce.c" CFLAGS "-fopenmp" LABELS serial openmp)
basilisk_add_ctest("parabola-explicit" SOURCE "parabola.c" CFLAGS "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("parabola-ml" SOURCE "parabola.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("seawall-ml" SOURCE "seawall.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("seawallsv" SOURCE "seawall.c" CFLAGS "-DSAINT_VENANT=1" LABELS serial)
basilisk_add_ctest("conicalsv" SOURCE "conical.c" CFLAGS "-DSAINT_VENANT=1" LABELS serial)
basilisk_add_ctest("conical-implicit" SOURCE "conical.c" CFLAGS "-DSAINT_VENANT=1" "-DIMPLICIT=1" LABELS serial)
basilisk_add_ctest("conical-ml" SOURCE "conical.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("explicit" SOURCE "implicit.c" CFLAGS "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("explicit-ml" SOURCE "implicit.c" CFLAGS "-DML=1" "-DEXPLICIT=1" LABELS serial)
basilisk_add_ctest("implicit-ml" SOURCE "implicit.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("explosion.3D" SOURCE "explosion.c" CFLAGS "-grid=octree" LABELS serial 3D)
basilisk_add_ctest("explosion3D" SOURCE "explosion.c" CFLAGS "-grid=multigrid3D" LABELS serial 3D)
basilisk_add_ctest("lidmac" SOURCE "lid.c" CFLAGS "-DMAC=1" LABELS serial)
basilisk_add_ctest("oscillation-compressible" SOURCE "oscillation.c" CFLAGS "-DCOMPRESSIBLE=1" LABELS serial)
basilisk_add_ctest("oscillation-momentum" SOURCE "oscillation.c" CFLAGS "-DMOMENTUM=1" LABELS serial)
basilisk_add_ctest("ponds-implicit" SOURCE "ponds.c" CFLAGS "-DIMPLICIT=1" LABELS serial)
basilisk_add_ctest("ponds-ml" SOURCE "ponds.c" CFLAGS "-DML=1" LABELS serial)
basilisk_add_ctest("poiseuille-fenep" SOURCE "poiseuille-oldroydb.c" CFLAGS "-DFENE_P=1" LABELS serial)
basilisk_add_ctest("rising-axi" SOURCE "rising.c" CFLAGS "-DAXIS=1" LABELS serial axi)
basilisk_add_ctest("rising-axi-momentum" SOURCE "rising.c" CFLAGS "-DAXIS=1" "-DMOMENTUM=1" LABELS serial axi)
basilisk_add_ctest("rising-axi-clsvof" SOURCE "rising.c" CFLAGS "-DAXIS=1" "-DLEVELSET=1" "-DCLSVOF=1" LABELS serial axi)
basilisk_add_ctest("rising2" SOURCE "rising.c" CFLAGS "-DCASE2=1" LABELS serial)
basilisk_add_ctest("rising2-levelset" SOURCE "rising.c" CFLAGS "-DCASE2=1" "-DLEVELSET=1" LABELS serial)
basilisk_add_ctest("rising2-clsvof" SOURCE "rising.c" CFLAGS "-DCASE2=1" "-DLEVELSET=1" "-DCLSVOF=1" LABELS serial)
basilisk_add_ctest("rising-reduced" SOURCE "rising.c" CFLAGS "-DREDUCED=1" LABELS serial)
basilisk_add_ctest("rising-levelset" SOURCE "rising.c" CFLAGS "-DLEVELSET=1" LABELS serial)
basilisk_add_ctest("rising-clsvof" SOURCE "rising.c" CFLAGS "-DLEVELSET=1" "-DCLSVOF=1" LABELS serial)
basilisk_add_ctest("rt-reduced" SOURCE "rt.c" CFLAGS "-DREDUCED=1" LABELS serial)
basilisk_add_ctest("taylor-green-all-mach" SOURCE "taylor-green.c" CFLAGS "-DALL_MACH=1" LABELS serial)
basilisk_add_ctest("facefraction3D" SOURCE "facefraction.c" CFLAGS "-grid=multigrid3D" LABELS serial 3D)

foreach(test_3d circle curvature hf periodic poisson refineu solenoidal)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_3d}.3D.ref")
    basilisk_add_ctest("${test_3d}.3D" SOURCE "${test_3d}.c" CFLAGS "-grid=octree" LABELS serial 3D)
  endif()
endforeach()

basilisk_add_ctest("dispersion-gn" SOURCE "dispersion.c" LABELS serial)
basilisk_add_ctest("dispersion" SOURCE "dispersion.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("bar-ml" SOURCE "bar.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("beach-ml" SOURCE "beach.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("wind-driven-nh" SOURCE "wind-driven.c" CFLAGS "-DML=1" "-DNH=1" LABELS serial multilayer)
basilisk_add_ctest("wind-driven" SOURCE "wind-driven.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("wind-driven-stvt" SOURCE "wind-driven.c" LABELS serial multilayer)
basilisk_add_ctest("gaussian-hydro" SOURCE "gaussian.c" CFLAGS "-DML=1" "-DHYDRO=1" LABELS serial multilayer)
basilisk_add_ctest("gaussian-nometric" SOURCE "gaussian.c" CFLAGS "-DML=1" "-DNOMETRIC=1" LABELS serial multilayer)
basilisk_add_ctest("gaussian-stvt" SOURCE "gaussian.c" LABELS serial multilayer)
basilisk_add_ctest("gaussian" SOURCE "gaussian.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("lonlat-ml" SOURCE "lonlat.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("nonlinear-ml" SOURCE "nonlinear.c" CFLAGS "-DML=1" LABELS serial multilayer)
basilisk_add_ctest("kh-hydro" SOURCE "kh.c" CFLAGS "-DHYDRO=1" LABELS serial multilayer)
basilisk_add_ctest("overflow-isopycnal" SOURCE "overflow.c" CFLAGS "-DISOPYCNAL=1" LABELS serial multilayer)
basilisk_add_ctest("shockwave" SOURCE "shockwave.c" CFLAGS "-grid=bitree" LABELS serial)
basilisk_add_ctest("shrinking" SOURCE "shrinking.c" CFLAGS "-grid=multigrid1D" LABELS serial)
basilisk_add_ctest("shrinking-axi" SOURCE "shrinking.c" CFLAGS "-DAXIS=1" "-grid=quadtree" LABELS serial axi)
basilisk_add_ctest("periodic2" SOURCE "periodic1.c" CFLAGS "-DTRASH=1" LABELS serial)
basilisk_add_ctest("periodic1" SOURCE "periodic1.c" CFLAGS "-grid=bitree" "-DTRASH=1" LABELS serial)
basilisk_add_ctest("coarsen" SOURCE "coarsen.c" CFLAGS "-progress" LABELS serial)
basilisk_add_ctest("hf-mask" SOURCE "hf-mask.c" CFLAGS "-DTRASH=1" LABELS serial)
basilisk_add_ctest("dirichlet" SOURCE "neumann.c" CFLAGS "-DDIRICHLET=1" LABELS serial embed)
basilisk_add_ctest("dirichlet-axi" SOURCE "neumann-axi.c" CFLAGS "-DDIRICHLET=1" LABELS serial embed axi)
basilisk_add_ctest("dirichlet3D" SOURCE "neumann3D.c" CFLAGS "-DDIRICHLET=1" LABELS serial embed 3D)
basilisk_add_ctest("ows_papa-kpp" SOURCE "ows_papa.c" CFLAGS "-DKPP=1" LABELS serial gotm)
basilisk_add_ctest("stencils" SOURCE "stencils.c" CFLAGS "-DPRINTBOUNDARY=1" LABELS serial)
basilisk_add_ctest("capwave-clsvof" SOURCE "capwave.c" CFLAGS "-DCLSVOF=1" LABELS serial)

# GPU backends are opt-in because they require backend-specific toolchains or
# runtime support beyond the default serial suite.
if(BASILISK_ENABLE_GLSL_TESTS)
  if(NOT BASILISK_USE_GLSL)
    message(FATAL_ERROR
      "BASILISK_ENABLE_GLSL_TESTS requires BASILISK_USE_GLSL=ON")
  endif()

  set(glsl_test_flags "-grid=gpu/multigrid" "-DPRINTNSHADERS")

  basilisk_add_ctest("gpu.gpu" SOURCE "gpu.c" REF "gpu.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("gpu-cpu.gpu" SOURCE "gpu.c" REF "gpu-cpu.gpu.ref"  CFLAGS ${glsl_test_flags} "-cpu" LABELS gpu glsl)
  basilisk_add_ctest("gpu-reduce.gpu" SOURCE "gpu-reduce.c"  REF "gpu-reduce.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("gpu-boundary.gpu" SOURCE "gpu-boundary.c" REF "gpu-boundary.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("restriction1.gpu" SOURCE "restriction1.c" REF "restriction1.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("layers.gpu" SOURCE "layers.c" REF "layers.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("nonlinear.gpu" SOURCE "nonlinear.c" REF "nonlinear.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("advection.gpu" SOURCE "advection.c" REF "advection.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("bump2D-gpu" SOURCE "bump2D-gpu.c"  REF "bump2D-gpu.ref" CFLAGS "-DPRINTNSHADERS" LABELS gpu glsl)
  basilisk_add_ctest("reversed.gpu" SOURCE "reversed.c" REF "reversed.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("axi.gpu" SOURCE "axi.c" REF "axi.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("lid.gpu" SOURCE "lid.c" REF "lid.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("stommel-ml.gpu" SOURCE "stommel-ml.c" REF "stommel-ml.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("bleck.gpu" SOURCE "bleck.c" REF "bleck.gpu.ref" CFLAGS ${glsl_test_flags} LABELS gpu glsl)
  basilisk_add_ctest("rising-clsvof.gpu" SOURCE "rising.c" REF "rising-clsvof.gpu.ref" CFLAGS ${glsl_test_flags} "-DLEVELSET=1" "-DCLSVOF=1" LABELS gpu glsl)
endif()

if(BASILISK_ENABLE_CUDA_TESTS)
  if(NOT BASILISK_USE_CUDA)
    message(FATAL_ERROR
      "BASILISK_ENABLE_CUDA_TESTS requires BASILISK_USE_CUDA=ON")
  endif()

  set(cuda_test_flags "-grid=cuda/multigrid" "-DPRINTNSHADERS")
  basilisk_add_ctest("gpu.cuda" SOURCE "gpu.c" REF "gpu.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
  basilisk_add_ctest("gpu-reduce.cuda" SOURCE "gpu-reduce.c" REF "gpu-reduce.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
  basilisk_add_ctest("restriction1.cuda" SOURCE "restriction1.c" REF "restriction1.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
  basilisk_add_ctest("gpu-boundary.cuda" SOURCE "gpu-boundary.c" REF "gpu-boundary.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
  basilisk_add_ctest("bump2D-gpu.cuda" SOURCE "bump2D-gpu.c" REF "bump2D-gpu.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
  basilisk_add_ctest("advection.cuda" SOURCE "advection.c" REF "advection.cuda.ref" CFLAGS ${cuda_test_flags} LABELS gpu cuda)
endif()

if(BASILISK_ENABLE_HIP_TESTS)
  if(NOT BASILISK_USE_HIP)
    message(FATAL_ERROR
      "BASILISK_ENABLE_HIP_TESTS requires BASILISK_USE_HIP=ON")
  endif()

  set(hip_test_flags "-grid=hip/multigrid" "-DPRINTNSHADERS")
  basilisk_add_ctest("gpu.hip" SOURCE "gpu.c" REF "gpu.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
  basilisk_add_ctest("gpu-reduce.hip" SOURCE "gpu-reduce.c" REF "gpu-reduce.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
  basilisk_add_ctest("restriction1.hip" SOURCE "restriction1.c" REF "restriction1.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
  basilisk_add_ctest("gpu-boundary.hip" SOURCE "gpu-boundary.c" REF "gpu-boundary.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
  basilisk_add_ctest("bump2D-gpu.hip" SOURCE "bump2D-gpu.c" REF "bump2D-gpu.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
  basilisk_add_ctest("advection.hip" SOURCE "advection.c" REF "advection.hip.ref" CFLAGS ${hip_test_flags} LABELS gpu hip)
endif()

# MPI tests are opt-in and carry their rank count as a qcc command-line
# definition; qcc needs to see -D_MPI itself before generating C code.
if(BASILISK_ENABLE_MPI_TESTS)
  find_program(BASILISK_MPICC mpicc)
  find_program(BASILISK_MPIRUN mpirun)
  if(NOT BASILISK_MPICC OR NOT BASILISK_MPIRUN)
    message(FATAL_ERROR
      "BASILISK_ENABLE_MPI_TESTS requires both mpicc and mpirun")
  endif()
  string(REPLACE "${CMAKE_C_COMPILER}" "${BASILISK_MPICC}"
    BASILISK_MPI_CC99 "${CC99}")

  function(basilisk_add_mpi_ctest name ranks)
    set(mpi_args ${ARGN})
    list(APPEND mpi_args CFLAGS "-D_MPI=${ranks}")
    basilisk_add_ctest("${name}"
      ${mpi_args}
      CC "${BASILISK_MPI_CC99}"
      EXECUTOR "${BASILISK_MPIRUN} --oversubscribe -np ${ranks}"
      MPI_RANKS "${ranks}"
      LABELS mpi
    )
  endfunction()

  basilisk_add_mpi_ctest("indexing" 3 SOURCE "indexing.c")
  basilisk_add_mpi_ctest("indexing.3D" 3 SOURCE "indexing.c" CFLAGS "-grid=octree" LABELS 3D)
  basilisk_add_mpi_ctest("mpi-restriction" 3 SOURCE "mpi-restriction.c")
  basilisk_add_mpi_ctest("mpi-restriction.3D" 3 SOURCE "mpi-restriction.c" CFLAGS "-grid=octree" LABELS 3D)
  basilisk_add_mpi_ctest("mpi-reduce" 3 SOURCE "mpi-reduce.c")
  basilisk_add_mpi_ctest("mpi-refine" 4 SOURCE "mpi-refine.c")
  basilisk_add_mpi_ctest("mpi-refine1" 11 SOURCE "mpi-refine1.c")
  basilisk_add_mpi_ctest("mpi-laplacian" 3 SOURCE "mpi-laplacian.c")
  basilisk_add_mpi_ctest("mpi-laplacian.3D" 3 SOURCE "mpi-laplacian.c" CFLAGS "-grid=octree" LABELS 3D)
  basilisk_add_mpi_ctest("mpi-circle" 5 SOURCE "mpi-circle.c")
  basilisk_add_mpi_ctest("mpi-circle1" 6 SOURCE "mpi-circle.c")
  basilisk_add_mpi_ctest("mpi-flux" 6 SOURCE "mpi-flux.c")
  basilisk_add_mpi_ctest("mpi-interpu" 5 SOURCE "mpi-interpu.c")
  basilisk_add_mpi_ctest("mpi-coarsen" 2 SOURCE "mpi-coarsen.c")
  basilisk_add_mpi_ctest("mpi-coarsen1" 5 SOURCE "mpi-coarsen1.c")
  basilisk_add_mpi_ctest("poiseuille-periodic" 4 SOURCE "poiseuille-periodic.c")
  basilisk_add_mpi_ctest("source" 7 SOURCE "source.c")
  basilisk_add_mpi_ctest("tag" 7 SOURCE "tag.c")
  basilisk_add_mpi_ctest("tag1" 7 SOURCE "tag1.c")
  basilisk_add_mpi_ctest("view" 4 SOURCE "view.c")
  basilisk_add_mpi_ctest("view.3D" 4 SOURCE "view.c" CFLAGS "-grid=octree" LABELS 3D)
  basilisk_add_mpi_ctest("boundary_vertex" 7 SOURCE "boundary_vertex.c")
  basilisk_add_mpi_ctest("boundary_vertex3D" 7 SOURCE "boundary_vertex3D.c")
  basilisk_add_mpi_ctest("foreach_bnd1" 4 SOURCE "foreach_bnd1.c")
  basilisk_add_mpi_ctest("vertices-bc" 4 SOURCE "vertices-bc.c")
  basilisk_add_mpi_ctest("reductions" 4 SOURCE "reductions.c")
  basilisk_add_mpi_ctest("kh-ns" 8 SOURCE "kh-ns.c")
  basilisk_add_mpi_ctest("hydrostatic2" 8 SOURCE "hydrostatic2.c")
  basilisk_add_mpi_ctest("vortex" 7 SOURCE "vortex.c" CFLAGS "-DJACOBI=1")
  basilisk_add_mpi_ctest("axiadvection" 7 SOURCE "axiadvection.c" CFLAGS "-DJACOBI=1")
endif()

add_custom_target(basilisk-check
  COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
  DEPENDS qcc
  USES_TERMINAL
)
