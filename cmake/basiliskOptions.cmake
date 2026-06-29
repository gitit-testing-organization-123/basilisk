option(BASILISK_USE_PPR "Build the PPR remapping library" OFF)
option(BASILISK_USE_KDT "Build the KDT terrain library and tools" OFF)
option(BASILISK_USE_CVMIX "Build the CVMix C interface" OFF)

option(BASILISK_USE_CUDA "Build the CUDA GPU backend" OFF)
option(BASILISK_USE_GLSL "Build the OpenGL/GLSL GPU backend" OFF)
option(BASILISK_USE_HIP "Build the HIP GPU backend" OFF)

option(BASILISK_USE_TEST "Create basilisk test suite" ON)
option(BASILISK_ENABLE_MPI_TESTS "Enable MPI-backed Basilisk CTest tests" ON)
option(BASILISK_ENABLE_GLSL_TESTS "Enable GLSL/OpenGL-backed Basilisk CTest tests" OFF)
option(BASILISK_ENABLE_CUDA_TESTS "Enable CUDA-backed Basilisk CTest tests" OFF)
option(BASILISK_ENABLE_HIP_TESTS "Enable HIP-backed Basilisk CTest tests" OFF)
