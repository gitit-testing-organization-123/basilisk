#include "reduce.h"

#pragma autolink -L$BASILISK/grid/cuda -lreduce

double gpu_reduction (size_t offset,
                      const char op,
                      const RegionParameters * region,
                      size_t nb)
{
  if (region->n.x == 1 && region->n.y == 1) {
    int i = (region->p.x - X0)/L0*N;
    int j = (region->p.y - Y0)/L0*N;
    if (i < 0 || i >= N || j < 0 || j >= N)
      return 0.;
    offset += i*N + j;
    nb = 1;
  }
  return cuda_reduce (GPUContext.ssbo + offset*sizeof(real), nb, op);
}
