#include "reduce.h"
#include <nvrtc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define CHECK_CUDA(x)                                                   \
    do {                                                                \
        CUresult err = (x);                                             \
        if (err != CUDA_SUCCESS) {                                      \
            const char* name = NULL;                                    \
            const char* str = NULL;                                     \
            cuGetErrorName(err, &name);                                 \
            cuGetErrorString(err, &str);                                \
            fprintf(stderr,                                             \
                    "CUDA ERROR: %s : %s\n",                            \
                    name ? name : "unknown",                            \
                    str ? str : "unknown");                             \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (0)

#define CHECK_NVRTC(x)                                                  \
    do {                                                                \
        nvrtcResult err = (x);                                          \
        if (err != NVRTC_SUCCESS) {                                     \
          fprintf(stderr,                                               \
                    "NVRTC ERROR: %s\n",                                \
                    nvrtcGetErrorString(err));                          \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (0)

static char kernel_source[] =
"#define REDUCE(reduced,rhs) reduced += rhs                                          \n"
"extern \"C\"\n"
"__global__ void reduce (const float* input, float* output, int n){\n"
"    __shared__ float sdata[256];                       \n"
"    unsigned int tid = threadIdx.x;                    \n"
"    unsigned int i = blockIdx.x * blockDim.x * 2 + tid;\n"
"    float reduced = 0.0f;                              \n"
"    if (i < n)                                         \n"
"        REDUCE (reduced, input[i]);                    \n"
"    if (i + blockDim.x < n)                            \n"
"        REDUCE (reduced, input[i + blockDim.x]);       \n"
"    sdata[tid] = reduced;                              \n"
"    __syncthreads();                                   \n"
"                                                       \n"
"    for (unsigned int s = blockDim.x/2; s > 32; s >>= 1){\n"
"        if (tid < s)                                    \n"
"            REDUCE (sdata[tid], sdata[tid + s]);        \n"
"        __syncthreads();                                \n"
"    }                                                   \n"
"    if (tid < 32) {                                     \n"
"        volatile float* smem = sdata;                   \n"
"        REDUCE (smem[tid], smem[tid + 32]);             \n"
"        REDUCE (smem[tid], smem[tid + 16]);             \n"
"        REDUCE (smem[tid], smem[tid + 8]);              \n"
"        REDUCE (smem[tid], smem[tid + 4]);              \n"
"        REDUCE (smem[tid], smem[tid + 2]);              \n"
"        REDUCE (smem[tid], smem[tid + 1]);              \n"
"    }                                                   \n"
"    if (tid == 0)                                       \n"
"        output[blockIdx.x] = sdata[0];                  \n"
"}                                                       \n";

static CUfunction compile_kernel (const char * start, const char * op)
{
  nvrtcProgram prog;
  char * s = kernel_source + strlen("#define REDUCE(reduced,rhs) ");
  memcpy (s, op, strlen (op));
  s += strlen(op); while (*s != '\n') *s++ = ' ';
  s = strstr (kernel_source, "float reduced = ");
  s += strlen ("float reduced = ");
  memcpy (s, start, strlen (start));
  s += strlen(start); while (*s != '\n') *s++ = ' '; 
  CHECK_NVRTC (nvrtcCreateProgram (&prog, kernel_source, "reduce.cu",
                                   0, NULL, NULL));
  int major, minor;
  CUdevice cuDevice;
  CHECK_CUDA (cuDeviceGet(&cuDevice, 0));
  CHECK_CUDA (cuDeviceGetAttribute (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,
                                    cuDevice));
  CHECK_CUDA (cuDeviceGetAttribute (&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR,
                                    cuDevice));
  char arch[] = "--gpu-architecture=compute_86";
  sprintf (arch, "--gpu-architecture=compute_%d%d", major, minor);
  const char* options[] = {
    arch,
    "--std=c++11",
    "--use_fast_math"
  };
  if (nvrtcCompileProgram (prog, 3, options) != NVRTC_SUCCESS) {
    fputs (kernel_source, stderr);
    size_t log_size;
    CHECK_NVRTC (nvrtcGetProgramLogSize (prog, &log_size));
    if (log_size) {
      char * log = (char *)malloc (log_size);
      CHECK_NVRTC (nvrtcGetProgramLog (prog, log));
      fprintf (stderr, "%s:%d: %s\n", __FILE__, __LINE__, log);
      free (log);
    }
    exit (EXIT_FAILURE);
  }

  /*    Extract PTX    */
  size_t ptx_size;
  CHECK_NVRTC (nvrtcGetPTXSize (prog, &ptx_size));
  char * ptx = (char *) malloc (ptx_size);
  CHECK_NVRTC (nvrtcGetPTX (prog, ptx));
  CHECK_NVRTC (nvrtcDestroyProgram (&prog));

  /*    Load module    */
  CUmodule module;
  CHECK_CUDA (cuModuleLoadData (&module, ptx));
  free (ptx);

  CUfunction kernel;
  CHECK_CUDA(cuModuleGetFunction (&kernel, module, "reduce"));
  return kernel;
}

float cuda_reduce (CUdeviceptr d_input, const size_t N, const char op)
{
  /*    Compile kernel with NVRTC    */
  
  CUfunction kernel;
  switch (op) {
  case '+': {
    static CUfunction k = 0;
    if (!k) k = compile_kernel ("0.0f;", "reduced += rhs");
    kernel = k;
    break;
  }
  case 'm': {
    static CUfunction k = 0;
    if (!k) k = compile_kernel ("3e38f;", "reduced = min(reduced,rhs)");
    kernel = k;
    break;
  }
  case 'M': {
    static CUfunction k = 0;
    if (!k) k = compile_kernel ("-3e38f;", "reduced = max(reduced,rhs)");
    kernel = k;
    break;
  }
  default:
    assert (0);
  }

  /*    Allocate device memory    */

  static size_t Np = 0;
  static CUdeviceptr d_output_a = 0, d_output_b = 0;
  if (N > Np) {
    const size_t max_blocks = (N + 511)/512;
    if (d_output_a) {
      CHECK_CUDA (cuMemFree (d_output_a));
      CHECK_CUDA (cuMemFree (d_output_b));
    }
    CHECK_CUDA (cuMemAlloc (&d_output_a, max_blocks*sizeof(float)));
    CHECK_CUDA (cuMemAlloc (&d_output_b, max_blocks*sizeof(float)));
    Np = N;
  }
    
  /*    Multi-pass reduction    */

  CUdeviceptr input = d_input, output = d_output_a;
  size_t current_n = N;
  while (current_n > 1) {
    const size_t threads = 256;
    size_t blocks = (current_n + threads*2 - 1)/(threads*2);
    void * args[] = {&input, &output, &current_n};
    CHECK_CUDA (cuLaunchKernel (kernel,
                                blocks, 1, 1, threads,
                                1, 1, 0, 0,
                                args, NULL));
    CHECK_CUDA (cuCtxSynchronize());
    current_n = blocks;
    input = output;
    output = (output == d_output_a) ? d_output_b : d_output_a;
  }

  /*        Copy result back        */  
  float gpu_reduce;
  CHECK_CUDA (cuMemcpyDtoH (&gpu_reduce, input, sizeof(float)));
#if 0
  /*    Cleanup    */
  CHECK_CUDA(cuMemFree(d_output_a));
  CHECK_CUDA(cuMemFree(d_output_b));
  CHECK_CUDA(cuModuleUnload(module));
#endif
  return gpu_reduce;
}

#if CHECK
int main (void)
{
  size_t N = (1 << 20) + 17;

  float * host_input = (float *)malloc(N * sizeof(float));

  int i;

  for (i = 0; i < N; ++i)
    host_input[i] = 1.f;

  float cpu_sum = 0.0f;
  for (i = 0; i < N; ++i)
    cpu_sum += host_input[i];

  /*    Initialize CUDA Driver API    */
  
  {
    CHECK_CUDA(cuInit(0));

    CUdevice device;

    CHECK_CUDA(cuDeviceGet(&device, 0));

    CUcontext context;

    CHECK_CUDA(cuCtxCreate(&context, 0, device));
  }
  
  CUdeviceptr d_input;
  CHECK_CUDA (cuMemAlloc (&d_input, N * sizeof(float)));
  CHECK_CUDA (cuMemcpyHtoD (d_input, host_input, N * sizeof(float)));
  float gpu_sum = cuda_reduce (d_input, N, '+');
  
  printf("CPU sum: %.3f\n", cpu_sum);
  printf("GPU sum: %.3f\n", gpu_sum);
  
  if (fabs(cpu_sum - gpu_sum) < 1e-3f)
    printf("VALIDATION PASSED\n");
  else
    printf("VALIDATION FAILED\n");

  gpu_sum = cuda_reduce (d_input, N, '+');
  assert (fabs(cpu_sum - gpu_sum) < 1e-3f);
  
  free(host_input);

  return 0;
}
#endif // CHECK

