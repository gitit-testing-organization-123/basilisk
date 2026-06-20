#define GRIDNAME "Multigrid (gpu)"
#include "../gpu-multigrid.h"
#pragma autolink -lgpu -lerrors -lglfw -ldl

static void gpu_multigrid_methods()
{
  multigrid_methods();
  boundary_level = gpu_boundary_level;
}
