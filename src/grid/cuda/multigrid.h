#define GRIDNAME "Multigrid (cuda)"
#define _CUDA 1
#include "../gpu-multigrid.h"
#pragma autolink  -lbuda -lcuda -lnvrtc -lerrors

static void cuda_multigrid_methods()
{
  multigrid_methods();
  boundary_level = gpu_boundary_level;
}
