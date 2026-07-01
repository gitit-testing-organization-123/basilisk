#define GRIDNAME "Cartesian (cuda)"
#define _CUDA 1
#include "../gpu-cartesian.h"
#pragma autolink -lbuda -lcuda -lnvrtc -lerrors
               
static void cuda_cartesian_methods()
{
  cartesian_methods();
  boundary_level = gpu_boundary_level;
}
