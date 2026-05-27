#include <cuda.h>
#include <nvrtc.h>

#pragma autolink -L$BASILISK/grid/gpu -L/usr/local/cuda/lib64 -lnvrtc -lcuda -lerrors

#define CUDA_CHECK(x)                                                   \
  do {                                                                  \
    CUresult err = (x);                                                 \
    if (err != CUDA_SUCCESS) {                                          \
      const char *msg;                                                  \
      cuGetErrorName(err, &msg);                                        \
      fprintf(stderr, "%s:%d: CUDA error: %s\n", __FILE__, __LINE__, msg); \
      exit(1);                                                          \
    }                                                                   \
  } while(0)

#define NVRTC_CHECK(x)                          \
  do {                                          \
    nvrtcResult err = (x);                      \
    if (err != NVRTC_SUCCESS) {                 \
      fprintf(stderr, "%s:%d: NVRTC error: %s\n", __FILE__, __LINE__,   \
              nvrtcGetErrorString(err));        \
      exit(1);                                  \
    }                                           \
  } while(0)

static struct {
  ///// CUDA ////
  CUdevice dev;
  CUcontext ctx;
  CUdeviceptr ssbo;
  ///// GPU /////
  const bool fragment_shader;
  int current_shader;
  size_t current_size;
  //// for compatibility with the OpenGL version only ////
  const int nssbo, max_ssbo_size;
} GPUContext = {
  .current_shader = -1,
  .nssbo = 1,
  .max_ssbo_size = 0
};

typedef struct {
  CUdeviceptr location;
  int type, nd, local;
  size_t size;
  void * pointer;
} MyUniform;

typedef struct {
  unsigned ng[2], nwg[2];
  CUdeviceptr _data, csOrigin;
  MyUniform * uniforms;
  CUmodule module;
  CUfunction kernel;
} Shader;

void free_shader (Shader * s)
{
  free (s->uniforms);
}

#include <khash.h>

KHASH_MAP_INIT_INT(INT, Shader *)

typedef struct {
  GRIDPARENT parent;
  khash_t(INT) * shaders;
} GridGPU;

Shader * load_normal_shader (const char * fs)
{
  //  fputs (fs, stderr);
  
  nvrtcProgram prog;
  NVRTC_CHECK(nvrtcCreateProgram (&prog, fs,
                                  "kernel.cu",
                                  0,
                                  NULL,
                                  NULL
                                  ));

  const char *opts[] = {
    "--gpu-architecture=compute_52", // fixme: is this the best?
    "-default-device",
    "-diag-suppress=177"
  };

  nvrtcResult compile_res = nvrtcCompileProgram (prog, sizeof(opts)/sizeof(char *), opts);

  if (compile_res != NVRTC_SUCCESS) {
    size_t logSize;
    NVRTC_CHECK (nvrtcGetProgramLogSize (prog, &logSize));
    if (logSize > 1) {
      char * log = (char *) malloc (logSize);
      NVRTC_CHECK (nvrtcGetProgramLog (prog, log));
      fputs (log, stderr);
      char * error = gpu_errors (log, fs, NULL, "CUDA");
      fputs (error, stderr);
      sysfree (error);
      free (log);
    }
    return NULL;
  }

  // ------------------------------------------------------------
  // Get PTX
  // ------------------------------------------------------------

  size_t ptxSize;
  NVRTC_CHECK (nvrtcGetPTXSize (prog, &ptxSize));
  char ptx[ptxSize];
  NVRTC_CHECK (nvrtcGetPTX (prog, ptx));
  NVRTC_CHECK (nvrtcDestroyProgram (&prog));

  // ------------------------------------------------------------
  // Load PTX module
  // ------------------------------------------------------------

  Shader * shader = calloc (1, sizeof (Shader));
  CUDA_CHECK (cuModuleLoadData (&shader->module, ptx));
  CUDA_CHECK (cuModuleGetFunction (&shader->kernel, shader->module, "kernel"));
  return shader;
}

bool gpu_init_context()
{
  if (GPUContext.ctx)
    return false;
  CUDA_CHECK (cuInit (0));
  CUDA_CHECK (cuDeviceGet (&GPUContext.dev, 0));
  CUDA_CHECK (cuCtxCreate (&GPUContext.ctx, 0, GPUContext.dev));
  return true;
}

void gpu_free_context (GridGPU * gpu_grid)
{
#if 1
  if (GPUContext.ssbo) {
    CUDA_CHECK (cuMemFree (GPUContext.ssbo));
    GPUContext.ssbo = 0;
  }
  GPUContext.current_size = 0;
#else
  if (gpu_grid->reduct[0]) {
    GL_C (glDeleteBuffers (2, gpu_grid->reduct));
    for (int i = 0; i < 2; i++)
      gpu_grid->reduct[i] = 0;
  }
  if (GPUContext.nssbo) {
    GL_C (glDeleteBuffers (GPUContext.nssbo, GPUContext.ssbo));
    free (GPUContext.ssbo);
    GPUContext.ssbo = NULL;
    GPUContext.nssbo = 0;
  }
  GPUContext.current_size = 0;
#endif
}

@ifndef tracing
  @ def tracing(func, file, line) do {
    cuCtxSynchronize();
    tracing(func, file, line);
  } while(0) @
  @ def end_tracing(func, file, line) do {
    cuCtxSynchronize();
    end_tracing(func, file, line);
  } while(0) @
@endif
               
void realloc_ssbo()
{
  if (!datasize)
    return;
  size_t totalsize = field_size()*datasize;
  assert (totalsize > GPUContext.current_size);
  CUdeviceptr ptr;
  CUDA_CHECK (cuMemAlloc (&ptr, totalsize)); // fixme: allocates memory twice
  if (GPUContext.current_size > 0) {
    CUDA_CHECK (cuMemcpyDtoD (ptr, GPUContext.ssbo, GPUContext.current_size));
    CUDA_CHECK (cuMemFree (GPUContext.ssbo));
  }
  GPUContext.ssbo = ptr;
  GPUContext.current_size = totalsize;
}

trace
static void gpu_cpu_sync_scalar (scalar s, char * sep, SyncMode mode)
{
  assert ((mode == GPU_READ && s.gpu.stored < 0) ||
	  (mode == GPU_WRITE && s.gpu.stored > 0));
  if (s.gpu.stored > 0 && !(s.stencil.bc & s_centered))
    boundary ({s});
  CUDA_CHECK (cuCtxSynchronize());
  size_t size = (size_t)field_size()*sizeof(real), offset = s.i*size;
  char * cd = grid_data() + offset;
  CUdeviceptr gd = GPUContext.ssbo + offset;
  if (mode == GPU_READ)
    CUDA_CHECK (cuMemcpyDtoH (cd, gd, size));
  else if (mode == GPU_WRITE)
    CUDA_CHECK (cuMemcpyHtoD (gd, cd, size));
  else
    assert (false);
  if (sep)
    fprintf (stderr, "%s%s", sep, s.name);
  s.gpu.stored = 0;
}

trace
void reset_gpu (void * alist, double val)
{
  size_t size = (size_t)field_size()*sizeof(real);
  scalar * list = alist;
  for (scalar s in list)
    if (!is_constant(s)) {
      size_t offset = s.i*size, totalsize = max(s.block, 1)*size;
#if SINGLE_PRECISION
      float fval = val;
      uint32_t bits;
      memcpy (&bits, &fval, sizeof(bits));
      CUDA_CHECK (cuMemsetD32 (GPUContext.ssbo + offset, bits, totalsize/sizeof(float)));
#else
      fprintf (stderr, "%s:%d: error: not implemented yet\n");
#endif
      s.gpu.stored = -1;
    }
}

void finalize_shader (Shader * shader, External * externals, External * merged)
{
  /**
  Get the SSBO pointer. */

  size_t size;
  CUDA_CHECK (cuModuleGetGlobal(&shader->_data, &size, shader->module, "_data"));
  assert (size == sizeof (GPUContext.ssbo));

  /**
  Get the csOrigin pointer. */

  CUDA_CHECK (cuModuleGetGlobal(&shader->csOrigin, &size, shader->module, "csOrigin"));
  assert (size == 2*sizeof (int));
  
  /**
  ## Make list of uniforms */

  for (External * g = merged; g; g = g->next)
    g->used = 0;
  int index = 1;
  for (External * g = externals; g && g->name; g++)
    g->used = index++;
  int nuniforms = 0;
  for (const External * g = merged; g; g = g->next) {
    if (g->name[0] == '.') continue;
    if (IS_EXTERNAL_CONSTANT(g)) continue;
    if (g->type == sym_function_declaration || g->type == sym_function_definition) continue;
    if (g->type == sym_INT && (!strcmp (g->name, "N") ||
			       !strcmp (g->name, "nl") ||
			       !strcmp (g->name, "bc_period_x") ||
			       !strcmp (g->name, "bc_period_y")))
      continue;
    if (g->type == sym_INT ||
	g->type == sym_LONG ||
	g->type == sym_FLOAT ||
	g->type == sym_DOUBLE ||
	g->type == sym__COORD ||
	g->type == sym_COORD ||
	g->type == sym_BOOL ||
	g->type == sym_VEC4) {
      char * name = str_append (NULL, EXTERNAL_NAME (g));
#if 0      
      int location = glGetUniformLocation (s->id, name);
#else
      CUdeviceptr location = 0;
      size_t size;
      cuModuleGetGlobal (&location, &size, shader->module, name);
#endif
      if (location) {
        //        fprintf (stderr, "%s %d %ld\n", name, g->type, size);
	// not an array or just a one-dimensional array
	assert (!g->nd);
	assert (!g->data || ((int *)g->data)[1] == 0);
	int nd = g->data ? ((int *)g->data)[0] : 1;
        switch (g->type) {
        case sym_INT: case sym_LONG:
          assert (size == sizeof(int)*nd); break;
        case sym_FLOAT:
          assert (size == sizeof(float)*nd); break;
        case sym_VEC4:
          nd *= 4;
          assert (size == sizeof(float)*nd); break;
        case sym_BOOL:
          assert (size == sizeof(bool)*nd); break;
#if SINGLE_PRECISION
        case sym_DOUBLE:
          assert (size == sizeof(float)*nd); break;
        case sym__COORD:
          nd *= 2;
          assert (size == sizeof(float)*nd); break;
        case sym_COORD:
          nd *= 3;
          assert (size == sizeof(float)*nd); break;
#else // DOUBLE_PRECISION
        case sym_DOUBLE:
          assert (size == sizeof(double)*nd); break;
        case sym__COORD:
          nd *= 2;
          assert (size == sizeof(double)*nd); break;
        case sym_COORD:
          nd *= 3;
          assert (size == sizeof(double)*nd); break;
#endif // DOUBLE_PRECISION
        default:
          assert (false);
        }
	shader->uniforms = realloc (shader->uniforms, (nuniforms + 2)*sizeof(MyUniform));
	shader->uniforms[nuniforms] = (MyUniform){
	  .location = location, .type = g->type, .nd = nd, .size = size,
	  .local = g->global == 1 ? -1 : g->used - 1,
	  .pointer = g->global == 1 ? g->pointer : NULL };
	shader->uniforms[nuniforms + 1].type = 0;
	nuniforms++;
	// uniforms refering to local variables must be in the 'externals' local list
	assert (g->global == 1 || g->used);
      }
#if 1
      else
        fprintf (stderr, "%s not found\n", name);
#endif
      sysfree (name);
    }
  }
}

void post_setup_shader (Shader * shader, External * externals)
{
  
  /**
  Set SSBO pointer. */
  
  assert (GPUContext.ssbo);
  CUDA_CHECK (cuMemcpyHtoD (shader->_data, &GPUContext.ssbo, sizeof (GPUContext.ssbo)));

  /**
  ## Set uniforms */

  for (const MyUniform * g = shader->uniforms; g && g->type; g++) {
    void * pointer = g->pointer;
    if (!pointer) {
      assert (g->local >= 0);
      pointer = externals[g->local].pointer;
    }
    switch (g->type) {
    case sym_INT: case sym_FLOAT: case sym_VEC4: case sym_BOOL:
      CUDA_CHECK (cuMemcpyHtoD (g->location, pointer, g->size));
      break;
    case sym_LONG: {
      int p[g->nd];
      long * data = pointer;
      for (int i = 0; i < g->nd; i++)
	p[i] = data[i];
      CUDA_CHECK (cuMemcpyHtoD (g->location, p, g->size));
      break;
    }
#if SINGLE_PRECISION
    case sym_DOUBLE: case sym__COORD: case sym_COORD: {
      float p[g->nd];
      double * data = pointer;
      for (int i = 0; i < g->nd; i++)
	p[i] = data[i];
      CUDA_CHECK (cuMemcpyHtoD (g->location, p, g->size));
      break;
    }
#else // DOUBLE_PRECISION
    case sym_DOUBLE: case sym__COORD: case sym_COORD:
      CUDA_CHECK (cuMemcpyHtoD (g->location, pointer, g->size));
      break;
#endif // DOUBLE_PRECISION
    default:
      assert (false);
    }
  }
}

int run_shader (const Shader * shader, const RegionParameters * region)
{
  /**
  ## Render 

  If this is a `foreach_point()` iteration, we access a single point */

  int Nl = region->level > 0 ? 1 << (region->level - 1) : N/Dimensions.x;
  if (region->n.x == 1 && region->n.y == 1) {
    int csOrigin[] = {
      (region->p.x - X0)/L0*Nl*Dimensions.x,
      (region->p.y - Y0)/L0*Nl*Dimensions.x
    };
    CUDA_CHECK (cuMemcpyHtoD (shader->csOrigin, csOrigin, 2*sizeof(int)));
    assert (!GPUContext.fragment_shader);
    CUDA_CHECK (cuLaunchKernel (shader->kernel,
                                1, 1, 1,
                                1, 1, 1,
                                0, 0, NULL, NULL));
  }

  /**
  This is a region */
  
  else if (region->n.x || region->n.y) {
#if 1
    assert (false);
#else
    float vsScale[] = {
      (region->box[1].x - region->box[0].x)/L0,
      (region->box[1].y - region->box[0].y)/L0
    };
    float vsOrigin[] = { (region->box[0].x - X0)/L0, (region->box[0].y - Y0)/L0 };
    GL_C (glUniform2fv (1, 1, vsOrigin));
    GL_C (glUniform2fv (2, 1, vsScale));
    assert (GPUContext.fragment_shader);
    GL_C (glMemoryBarrier (GL_SHADER_STORAGE_BARRIER_BIT));
    GL_C (glDrawArrays (GL_TRIANGLES, 0, 6));
#endif
  }

  else {
    assert (!GPUContext.fragment_shader);
    CUDA_CHECK (cuLaunchKernel (shader->kernel,
                                shader->ng[0], shader->ng[1], 1,
                                shader->nwg[0], shader->nwg[1], 1,
                                0, 0, NULL, NULL));
  }
  return Nl;
}

void gpu_free_solver (void)
{
  CUDA_CHECK (cuCtxSynchronize ());
  CUDA_CHECK (cuCtxDestroy (GPUContext.ctx));
  GPUContext.ctx = NULL;
}

#undef device_synchronize
#define device_synchronize() cuCtxSynchronize()
