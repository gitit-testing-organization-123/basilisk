#include <cuda.h>

float cuda_reduce (CUdeviceptr d_input, const size_t N, const char op);
