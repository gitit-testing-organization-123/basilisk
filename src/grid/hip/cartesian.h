#define GRIDNAME "Cartesian (hip)"
#define _CUDA 1
#include "../gpu-cartesian.h"

/**
Note that the libraries `-lcuda -lcudart -lnvrtc` are only required for the
HIP-on-CUDA version. This will need to be adapted for the HIP/ROCm
version. See also the [Makefile]() for compilation of the
corresponding `libhip.a` library. */

#pragma autolink -lhip -lcuda -lcudart -lnvrtc -lerrors
               
static void hip_cartesian_methods()
{
  cartesian_methods();
  boundary_level = gpu_boundary_level;
}
