#include <grid/gpu/glad.h>
#include <GLFW/glfw3.h>
#if DEBUG_OPENGL
#include <grid/gpu/debug.h>
#endif

#pragma autolink -L$BASILISK/grid/gpu -lglfw -lgpu -lerrors -ldl

static struct {
  ///// GPU /////
  GLFWwindow * window;
  GLuint * ssbo;
  bool fragment_shader;
  int current_shader, nssbo;
  size_t max_ssbo_size, current_size;
} GPUContext = {
  .current_shader = -1,
};

static void gpu_check_error (const char * stmt, const char * fname, int line)
{
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf (stderr, "%s:%d: error: OpenGl %08x for '%s;'\n",
	     fname, line, err, stmt);
    abort();
  }
}

#ifdef NDEBUG
// helper macro that checks for GL errors.
#define GL_C(stmt) do {	stmt; } while (0)
#else
// helper macro that checks for GL errors.
#define GL_C(stmt) do {							\
    stmt;								\
    gpu_check_error (#stmt, __FILE__, LINENO);				\
  } while (0)
#endif

void gpu_free_solver (void)
{
  GL_C (glFinish());
  GL_C (glBindFramebuffer (GL_FRAMEBUFFER, 0));
  glDeleteBuffers (GPUContext.nssbo, GPUContext.ssbo);
  free (GPUContext.ssbo);
  glfwTerminate();
  GPUContext.window = NULL;
}

static char * getShaderLogInfo (GLuint shader)
{
  char * infoLog = NULL;
  GLint len;
  GL_C (glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &len));
  if (len > 0) {
    GLsizei actualLen;
    infoLog = malloc (len);
    GL_C (glGetShaderInfoLog (shader, len, &actualLen, infoLog));
  }
  return infoLog;
}

static char * getProgramLogInfo (GLuint program)
{
  char * infoLog = NULL;
  GLint len;
  GL_C (glGetProgramiv (program, GL_INFO_LOG_LENGTH, &len));
  if (len > 0) {
    GLsizei actualLen;
    infoLog = malloc (len);
    GL_C (glGetProgramInfoLog (program, len, &actualLen, infoLog));
  }
  return infoLog;
}

static GLuint createShaderFromString (const char * shaderSource,
				      const GLenum shaderType)
{
  GLuint shader;

  GL_C (shader = glCreateShader(shaderType));
  GL_C (glShaderSource (shader, 1, &shaderSource, NULL));
  GL_C (glCompileShader (shader));

  GLint compileStatus;
  GL_C (glGetShaderiv (shader, GL_COMPILE_STATUS, &compileStatus));
  if (compileStatus != GL_TRUE) {
    char * info = getShaderLogInfo (shader);
#if PRINTSHADERERROR
    fputs (shaderSource, stderr);
    fputs (info, stderr);
#endif
    char * error = gpu_errors (info, shaderSource, NULL, "GLSL");
    fputs (error, stderr);
    sysfree (error);
    free (info);
    glDeleteShader (shader);
    return 0;
  }

  return shader;
}

static GLuint loadNormalShader (const char * vsSource, const char * fsShader)
{
  GLuint vs = 0;
  if (vsSource) {
    vs = createShaderFromString (vsSource, GL_VERTEX_SHADER);
    if (!vs)
      return 0;
  }
  GLuint fs = createShaderFromString (fsShader, vsSource ? GL_FRAGMENT_SHADER : GL_COMPUTE_SHADER);
  if (!fs)
    return 0;
  
  GLuint shader = glCreateProgram();
  if (vs)
    glAttachShader (shader, vs);
  glAttachShader (shader, fs);
  glLinkProgram (shader);

  GLint Result;
  glGetProgramiv (shader, GL_LINK_STATUS, &Result);

  if (Result == GL_FALSE) {
    char * info = getProgramLogInfo (shader);
    fprintf (stderr, "GLSL: could not link shader \n\n%s\n%s\n%s\n",
	     info, vsSource, fsShader);
    free (info);
    glDeleteProgram (shader);
    shader = 0;
  }

  if (shader) {
    if (vs)
      glDetachShader (shader, vs);
    glDetachShader (shader, fs);
  }

  if (vs)
    glDeleteShader (vs);
  glDeleteShader (fs);
  
  return shader;
}

typedef struct {
  char * s;
  int index;
} GLString;

GLString gpu_limits_list[] = {
  {"GL_MAX_DRAW_BUFFERS", GL_MAX_DRAW_BUFFERS},
  {"GL_MAX_VERTEX_UNIFORM_COMPONENTS", GL_MAX_VERTEX_UNIFORM_COMPONENTS},
  {"GL_MAX_VERTEX_UNIFORM_BLOCKS", GL_MAX_VERTEX_UNIFORM_BLOCKS},
  {"GL_MAX_VERTEX_OUTPUT_COMPONENTS", GL_MAX_VERTEX_OUTPUT_COMPONENTS},
  {"GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_VERTEX_OUTPUT_COMPONENTS", GL_MAX_VERTEX_OUTPUT_COMPONENTS},
#if 1  
  {"GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS", GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS},
  {"GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS", GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS},
  {"GL_MAX_TESS_CONTROL_INPUT_COMPONENTS", GL_MAX_TESS_CONTROL_INPUT_COMPONENTS},
  {"GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS", GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS},
  {"GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS", GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS", GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS},
  {"GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS", GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS},
  {"GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS", GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS},
  {"GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS", GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS},
  {"GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS", GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS},
  {"GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS", GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS", GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS},
#endif
#if 1  
  {"GL_MAX_COMPUTE_UNIFORM_COMPONENTS", GL_MAX_COMPUTE_UNIFORM_COMPONENTS},
  {"GL_MAX_COMPUTE_UNIFORM_BLOCKS", GL_MAX_COMPUTE_UNIFORM_BLOCKS},
  {"GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS", GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_SHADER_STORAGE_BLOCK_SIZE", GL_MAX_SHADER_STORAGE_BLOCK_SIZE},
  {"GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS", GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS},
  {"GL_MAX_TEXTURE_BUFFER_SIZE", GL_MAX_TEXTURE_BUFFER_SIZE},
#endif
  {"GL_MAX_GEOMETRY_UNIFORM_COMPONENTS", GL_MAX_GEOMETRY_UNIFORM_COMPONENTS},
  {"GL_MAX_GEOMETRY_UNIFORM_BLOCKS", GL_MAX_GEOMETRY_UNIFORM_BLOCKS},
  {"GL_MAX_GEOMETRY_INPUT_COMPONENTS", GL_MAX_GEOMETRY_INPUT_COMPONENTS},
  {"GL_MAX_GEOMETRY_OUTPUT_COMPONENTS", GL_MAX_GEOMETRY_OUTPUT_COMPONENTS},
  {"GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS", GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_GEOMETRY_OUTPUT_COMPONENTS", GL_MAX_GEOMETRY_OUTPUT_COMPONENTS},
  {"GL_MAX_FRAGMENT_UNIFORM_COMPONENTS", GL_MAX_FRAGMENT_UNIFORM_COMPONENTS},
  {"GL_MAX_FRAGMENT_UNIFORM_BLOCKS", GL_MAX_FRAGMENT_UNIFORM_BLOCKS},
  {"GL_MAX_FRAGMENT_INPUT_COMPONENTS", GL_MAX_FRAGMENT_INPUT_COMPONENTS},
  {"GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS},
  {"GL_MAX_FRAGMENT_IMAGE_UNIFORMS", GL_MAX_FRAGMENT_IMAGE_UNIFORMS},
  {"GL_MAX_IMAGE_UNITS", GL_MAX_IMAGE_UNITS},
  {NULL}
};

void printWorkGroupsCapabilities()
{
  int workgroup_size[3];
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &workgroup_size[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &workgroup_size[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &workgroup_size[2]);
  printf ("Maximal workgroup sizes:\n\tx:%u\n\ty:%u\n\tz:%u\n",
	  workgroup_size[0], workgroup_size[1], workgroup_size[2]);

  int workgroup_count[3];
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workgroup_count[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &workgroup_count[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &workgroup_count[2]);
  printf ("Maximum number of local invocations:\n\tx:%u\n\ty:%u\n\tz:%u\n",
	  workgroup_count[0], workgroup_count[1], workgroup_count[2]);

  int workgroup_invocations;
  glGetIntegerv (GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &workgroup_invocations);
  printf ("Maximum workgroup invocations:\n\t%u\n", workgroup_invocations);
}

typedef struct {
  int location, type, nd, local;
  void * pointer;
} MyUniform;

typedef struct {
  unsigned id, ng[2];
  MyUniform * uniforms;
} Shader;

void free_shader (Shader * s)
{
  free (s->uniforms);
}

#include <khash.h>

KHASH_MAP_INIT_INT(INT, Shader *)

typedef struct {
  GRIDPARENT parent;
  GLuint reduct[2];
  khash_t(INT) * shaders;
} GridGPU;

Shader * load_normal_shader (const char * fs)
{
  GLuint id;
  if (!GPUContext.fragment_shader)
    id = loadNormalShader (NULL, fs);
  else {
    const char quad[] =
      "#version 430\n"
      "layout(location = 0) in vec3 vsPos;"
      "out vec2 vsPoint;"
      "void main() {"
      "  vsPoint = vsPos.xy;"
      "  gl_Position =  vec4(2.*vsPos.xy - vec2(1.), 0., 1.);"
      "}";
    id = loadNormalShader (quad, fs);
  }
  Shader * shader = NULL;
  if (id) {
    shader = calloc (1, sizeof (Shader));
    shader->id = id;
  }
  return shader;
}

void gpu_limits (FILE * fp)
{
  GLString * i = gpu_limits_list;
  while (i->s) {
    GLint val;
    GL_C (glGetIntegerv (i->index, &val));  
    fprintf (fp, "%s: %d\n", i->s, val);
    i++;
  }
}

void gpu_free_context (GridGPU * gpu_grid)
{
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
}

bool gpu_init_context (GridGPU * gpu_grid)
{
  for (int i = 0; i < 2; i++)
    gpu_grid->reduct[i] = 0;
  
  if (GPUContext.window)
    return false;
  
  if (!glfwInit ())
    exit (1);

  glfwWindowHint (GLFW_VISIBLE, GL_FALSE);
  glfwWindowHint (GLFW_RESIZABLE, GL_FALSE);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint (GLFW_SAMPLES, 0);
  glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#if DEBUG_OPENGL    
  glfwWindowHint (GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
    
  GPUContext.window = glfwCreateWindow (1, 1, "GPU grid", NULL, NULL);
  if (!GPUContext.window) {
    glfwTerminate();
    fprintf (stderr, "GLFW: error: could not create window!\n");
    exit (1);
  }
  glfwMakeContextCurrent (GPUContext.window);

  // load GLAD.
  assert (gladLoadGLLoader ((GLADloadproc)glfwGetProcAddress));
  assert (glBindImageTexture);

#if DEBUG_OPENGL    
  GLint flags;
  GL_C (glGetIntegerv (GL_CONTEXT_FLAGS, &flags));
  if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
    GL_C (glEnable(GL_DEBUG_OUTPUT));
    GL_C (glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    GL_C (glDebugMessageCallback (GLDebugMessageCallback, NULL));
    GL_C (glDebugMessageControl (GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE));
  }
#endif // DEBUG_OPENGL

  return true;
}

@ifndef tracing
  @ def tracing(func, file, line) do {
    if (glFinish) glFinish();
    tracing(func, file, line);
  } while(0) @
  @ def end_tracing(func, file, line) do {
    if (glFinish) glFinish();
    end_tracing(func, file, line);
  } while(0) @
@endif

void realloc_ssbo()
{
  if (!datasize)
    return;
  
  GLint max_ssbo_size;
  GL_C (glGetIntegerv (GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_ssbo_size));
#if 1
  GPUContext.max_ssbo_size = 128*(max_ssbo_size/128);
#else // for testing multi SSBOs
  GPUContext.max_ssbo_size = 128*(5*field_size()/2*sizeof(float)/128);
#endif
  
  size_t totalsize = field_size()*datasize;
  int nssbo = totalsize/GPUContext.max_ssbo_size;
  if (nssbo*GPUContext.max_ssbo_size < totalsize)
    nssbo++;

  GLint max_ssbo_blocks;
  GL_C (glGetIntegerv (GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &max_ssbo_blocks));
  if (nssbo > max_ssbo_blocks) {
    fprintf (stderr,
	     "%s:%d: error: cannot allocate %ld bytes\n"
	     "%s:%d: error: maximum allowed is %d x %ld bytes\n",
	     __FILE__, LINENO, totalsize, __FILE__, LINENO,
	     max_ssbo_blocks, GPUContext.max_ssbo_size);
    exit (1);
  }

#if DEBUGALLOC
  fprintf (stderr, "resizing from %ld to %ld nssbo %d pnssbo %d max_ssbo %ld\n", 
	   GPUContext.current_size, totalsize, nssbo, GPUContext.nssbo, GPUContext.max_ssbo_size);
#endif
  assert (totalsize > GPUContext.current_size);
  size_t size = max(GPUContext.nssbo - 1, 0)*GPUContext.max_ssbo_size;
  size_t current_size = GPUContext.current_size - size;
  assert (current_size >= 0 && current_size <= GPUContext.max_ssbo_size);
  GPUContext.current_size = totalsize;
  totalsize -= size;
  assert (totalsize >= 0);

  if (current_size > 0) {
    size_t size = min (totalsize, GPUContext.max_ssbo_size);
    totalsize -= size;
    if (current_size < GPUContext.max_ssbo_size) {
      GLuint ssbo;
      GL_C (glGenBuffers (1, &ssbo));
      GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, ssbo));
      GL_C (glBufferData (GL_SHADER_STORAGE_BUFFER, size, NULL, GL_DYNAMIC_READ));
      GL_C (glBindBuffer (GL_COPY_READ_BUFFER, GPUContext.ssbo[GPUContext.nssbo - 1]));
      GL_C (glBindBuffer (GL_COPY_WRITE_BUFFER, ssbo));
      GL_C (glCopyBufferSubData (GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, current_size));
      GL_C (glDeleteBuffers (1, &GPUContext.ssbo[GPUContext.nssbo - 1]));
      GPUContext.ssbo[GPUContext.nssbo - 1] = ssbo;
    }
#if DEBUGALLOC    
    else
      fprintf (stderr, "  skipping fully allocated %ld\n", current_size);
#endif
  }

#if DEBUGALLOC
  fprintf (stderr, "  need to allocate %ld\n", totalsize);
#endif
  if (nssbo > GPUContext.nssbo) {
    assert (totalsize > 0);
    GPUContext.ssbo = realloc (GPUContext.ssbo, nssbo*sizeof(GLuint));
#if DEBUGALLOC
    fprintf (stderr, "  allocating %d buffers\n", nssbo - GPUContext.nssbo);
#endif
    GL_C (glGenBuffers (nssbo - GPUContext.nssbo, GPUContext.ssbo + GPUContext.nssbo));
    while (GPUContext.nssbo < nssbo) {
      GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, GPUContext.ssbo[GPUContext.nssbo]));
      size_t size = min (totalsize, GPUContext.max_ssbo_size);
#if DEBUGALLOC
      fprintf (stderr, "  allocating buffer %d size %ld\n", GPUContext.nssbo, size);
#endif
      GL_C (glBufferData (GL_SHADER_STORAGE_BUFFER, size, NULL, GL_DYNAMIC_READ));
      totalsize -= size;
      GPUContext.nssbo++;
    }
  }
  GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0));
#if DEBUGALLOC
  fprintf (stderr, "done resizing %d %ld %ld\n", GPUContext.nssbo, GPUContext.current_size, totalsize);
#endif  
  assert (totalsize == 0);
}

trace
static void gpu_cpu_sync_scalar (scalar s, char * sep, SyncMode mode)
{
  assert ((mode == GPU_READ && s.gpu.stored < 0) ||
	  (mode == GPU_WRITE && s.gpu.stored > 0));
  if (s.gpu.stored > 0 && !(s.stencil.bc & s_centered))
    boundary ({s});
  GL_C (glMemoryBarrier (GL_BUFFER_UPDATE_BARRIER_BIT));
  size_t size = (size_t)field_size()*sizeof(real), offset = s.i*size, totalsize = s.block*size;
  char * cd = grid_data() + offset;
  int index = offset/GPUContext.max_ssbo_size;
  offset -= index*GPUContext.max_ssbo_size;
  while (totalsize) {
    GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, GPUContext.ssbo[index]));
    size_t size = min (totalsize, GPUContext.max_ssbo_size - offset);

    //    fprintf (stderr, "map %d %ld %ld\n", index, offset, size);
    
    char * gd = glMapBufferRange (GL_SHADER_STORAGE_BUFFER, offset, size,
                                  mode == GPU_READ ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT);
    assert (gd);
    if (mode == GPU_READ)
      memcpy (cd, gd, size);
    else if (mode == GPU_WRITE)
      memcpy (gd, cd, size);
    else
      assert (false);
    assert (glUnmapBuffer (GL_SHADER_STORAGE_BUFFER));
    cd += size, totalsize -= size, offset = 0, index++;
  }
  GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0));
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
      int index = offset/GPUContext.max_ssbo_size;
      offset -= index*GPUContext.max_ssbo_size;
      while (totalsize) {
	GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, GPUContext.ssbo[index]));
	size_t size = min (totalsize, GPUContext.max_ssbo_size - offset);
#if SINGLE_PRECISION
	float fval = val;
	GL_C (glClearBufferSubData (GL_SHADER_STORAGE_BUFFER, GL_R32F,
				    offset, size,
				    GL_RED, GL_FLOAT, &fval));
#else
	GL_C (glClearBufferSubData (GL_SHADER_STORAGE_BUFFER, GL_RG32UI,
				    offset, size,
				    GL_RG_INTEGER, GL_UNSIGNED_INT, &val));      
#endif
	totalsize -= size, offset = 0, index++;
      }
      s.gpu.stored = -1;
    }
  GL_C (glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0));
}

void finalize_shader (Shader * s, External * externals, External * merged)
{
  
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
      int location = glGetUniformLocation (s->id, name);
      sysfree (name);
      if (location >= 0) {
	// fprintf (stderr, "%s:%d: %s\n", loop->fname, loop->line, name);
	// not an array or just a one-dimensional array
	assert (!g->nd);
	assert (!g->data || ((int *)g->data)[1] == 0);
	int nd = g->data ? ((int *)g->data)[0] : 1;
	s->uniforms = realloc (s->uniforms, (nuniforms + 2)*sizeof(MyUniform));
	s->uniforms[nuniforms] = (MyUniform){
	  .location = location, .type = g->type, .nd = nd,
	  .local = g->global == 1 ? -1 : g->used - 1,
	  .pointer = g->global == 1 ? g->pointer : NULL };
	s->uniforms[nuniforms + 1].type = 0;
	nuniforms++;
	// uniforms refering to local variables must be in the 'externals' local list
	assert (g->global == 1 || g->used);
      }
    }
  }
}

void post_setup_shader (Shader * shader, External * externals)
{
  
  /**
  For the Intel driver, it looks like the next line is necessary to
  ensure proper synchronisation of the compute shader and fragment
  shader (for example when using output_ppm() for interactive
  display). The nvidia driver somehow does not need this... */

  if (shader->id != GPUContext.current_shader) {
    GL_C (glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, 0));
    GL_C (glUseProgram (shader->id));
    for (int i = 0; i < GPUContext.nssbo; i++)
      GL_C (glBindBufferBase (GL_SHADER_STORAGE_BUFFER, i, GPUContext.ssbo[i]));
    GPUContext.current_shader = shader->id;
  }
    
  /**
  ## Set uniforms */

  for (const MyUniform * g = shader->uniforms; g && g->type; g++) {
    void * pointer = g->pointer;
    if (!pointer) {
      assert (g->local >= 0);
      pointer = externals[g->local].pointer;
    }
    switch (g->type) {
    case sym_INT:
      glUniform1iv (g->location, g->nd, pointer); break;
    case sym_FLOAT:
      glUniform1fv (g->location, g->nd, pointer); break;
    case sym_VEC4:
      glUniform4fv (g->location, g->nd, pointer); break;
    case sym_BOOL: {
      int p[g->nd];
      bool * data = pointer;
      for (int i = 0; i < g->nd; i++)
	p[i] = data[i];
      glUniform1iv (g->location, g->nd, p);
      break;
    }
    case sym_LONG: {
      int p[g->nd];
      long * data = pointer;
      for (int i = 0; i < g->nd; i++)
	p[i] = data[i];
      glUniform1iv (g->location, g->nd, p);
      break;
    }
#if SINGLE_PRECISION
    case sym_DOUBLE: {
      float p[g->nd];
      double * data = pointer;
      for (int i = 0; i < g->nd; i++)
	p[i] = data[i];
      glUniform1fv (g->location, g->nd, p);
      break;
    }
    case sym__COORD: {
      float p[2*g->nd];
      double * data = pointer;
      for (int i = 0; i < 2*g->nd; i++)
	p[i] = data[i];
      glUniform2fv (g->location, g->nd, p);
      break;
    }
    case sym_COORD: {
      float p[3*g->nd];
      double * data = pointer;
      for (int i = 0; i < 3*g->nd; i++)
	p[i] = data[i];
      glUniform3fv (g->location, g->nd, p);
      break;
    }
#else // DOUBLE_PRECISION
    case sym_DOUBLE:
      glUniform1dv (g->location, g->nd, pointer); break;
    case sym__COORD:
      glUniform2dv (g->location, g->nd, pointer); break;
    case sym_COORD:
      glUniform3dv (g->location, g->nd, pointer); break;
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

  If this is a `foreach_point()` iteration, we draw a single point */

  int Nl = region->level > 0 ? 1 << (region->level - 1) : N/Dimensions.x;
  if (region->n.x == 1 && region->n.y == 1) {
    int csOrigin[] = {
      (region->p.x - X0)/L0*Nl*Dimensions.x,
      (region->p.y - Y0)/L0*Nl*Dimensions.x
    };
    GL_C (glUniform2iv (0, 1, csOrigin));
    assert (!GPUContext.fragment_shader);
    GL_C (glMemoryBarrier (GL_SHADER_STORAGE_BARRIER_BIT));
    GL_C (glDispatchCompute (1, 1, 1));
  }

  /**
  This is a region */
  
  else if (region->n.x || region->n.y) {
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
  }

  else {
    assert (!GPUContext.fragment_shader);
    GL_C (glMemoryBarrier (GL_SHADER_STORAGE_BARRIER_BIT));
    GL_C (glDispatchCompute (shader->ng[0], shader->ng[1], 1));
  }

  return Nl;
}

#undef device_synchronize
#define device_synchronize() glFinish()
