char * gpu_errors (const char * errors, const char * source, char * fout,
                   const char * lang);

#if TRACE == 3
# define tracing_foreach(name, file, line) tracing(name, file, line)
# define end_tracing_foreach(name, file, line) end_tracing(name, file, line)
#else
# define tracing_foreach(name, file, line)
# define end_tracing_foreach(name, file, line)
#endif

macro2 BEGIN_FOREACH()
{
  if (_gpu_done_)
    _gpu_done_ = false;
  else {
    tracing_foreach ("foreach", S__FILE__, S_LINENO);
    {...}
    end_tracing_foreach ("foreach", S__FILE__, S_LINENO);
  }
}

typedef struct {
  coord p, * box, n; // region
  int level; // level
} RegionParameters;

bool gpu_end_stencil (ForeachData * loop, const RegionParameters * region,
		      External * externals, const char * kernel);

macro2 foreach_stencil_generic (char flags, Reduce reductions,
				int _parallel, External * _externals, const char * _kernel)
{
  tracing_foreach ("foreach", S__FILE__, S_LINENO);
  static ForeachData _loop = { .fname = S__FILE__, .line = S_LINENO, .first = 1 };
  _loop.parallel = _parallel;
  if (baseblock)
    for (scalar s = baseblock[0], * i = baseblock; s.i >= 0; i++, s = *i) {
      _attribute[s.i].stencil.io = 0;
      _attribute[s.i].stencil.width = 0;
    }
  int ig = 0, jg = 0, kg = 0; NOT_UNUSED(ig); NOT_UNUSED(jg); NOT_UNUSED(kg);
  Point point = {0}; NOT_UNUSED (point);
  RegionParameters _region = {0};
  
  {...}
  
#if PRINTIO
  if (baseblock) {
    fprintf (stderr, "%s:%d:", _loop.fname, _loop.line);
    for (scalar s = baseblock[0], * i = baseblock; s.i >= 0; i++, s = *i)
      if ((_attribute[s.i].stencil.io & s_input) || (_attribute[s.i].stencil.io & s_output))
	fprintf (stderr, " %s:%d:%c:%d", _attribute[s.i].name, s.i,
                 (_attribute[s.i].stencil.io & s_input) &&
                 (_attribute[s.i].stencil.io & s_output) ? 'a' :
		 (_attribute[s.i].stencil.io & s_input) ? 'r' : 'w',
		 _attribute[s.i].stencil.width);
    fprintf (stderr, "\n");
  }
#endif // PRINTIO
  bool _first = _loop.first;
  _loop.first = 0; // to avoid warnings in check_stencil
  check_stencil (&_loop);
  _loop.first = _first;
  _gpu_done_ = gpu_end_stencil (&_loop, &_region, _externals, _kernel);
  _loop.first = 0;
  end_tracing_foreach ("foreach", S__FILE__, S_LINENO);
}

macro2 foreach_stencil (char flags, Reduce reductions,
			int _parallel, External * _externals, const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel)
    {...}
}

macro2 foreach_level_stencil (int _level, char flags, Reduce reductions,
			      int _parallel, External * _externals, const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel) {
    _region.level = _level + 1;
    {...}
  }
}

macro2 foreach_point_stencil (double _xp, double _yp, double _zp,
			      char flags, Reduce reductions,
			      int _parallel, External * _externals, const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel) {
    _region.p = (coord){ _xp, _yp, _zp };
    _region.n = (coord){ 1, 1 };
    {...}
  }
}

macro2 foreach_region_stencil (coord _p, coord _box[2], coord _n,
			       char flags, Reduce reductions,
			       int _parallel, External * _externals, const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel) {
    _region.p = _p, _region.box = _box, _region.n = _n;
    {...}
  }
}

macro2 foreach_vertex_stencil (char flags, Reduce reductions,
				  int _parallel, External * _externals,
				  const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel) {
    _loop.vertex = true;
    {...}
  }
}

macro2 foreach_face_stencil (char flags, Reduce reductions, const char * _order,
			     int _parallel, External * _externals,
			     const char * _kernel)
{
  foreach_stencil_generic (flags, reductions, _parallel, _externals, _kernel)
    {...}
}

macro2 foreach_coarse_level_stencil (int _level, char flags, Reduce reductions,
				     int _parallel, External * _externals,
				     const char * _kernel)
{
  foreach_level_stencil (_level, flags, reductions, _parallel, _externals, _kernel)
    {...}
}

macro2 foreach_level_or_leaf_stencil (int _level, char flags, Reduce reductions,
				      int _parallel, External * _externals,
				      const char * _kernel)
{
  foreach_level_stencil (_level, flags, reductions, _parallel, _externals, _kernel)
    {...}
}

typedef enum {
  GPU_READ, GPU_WRITE
} SyncMode;

static void gpu_cpu_sync_scalar (scalar s, char * sep, SyncMode mode);

void realloc_scalar_gpu (int size)
{
  realloc_scalar (size);
  void realloc_ssbo();
  realloc_ssbo();
}

void gpu_boundary_level (scalar * list, int l)
{
  scalar * list1 = NULL;
  for (scalar s in list)
    if (s.gpu.stored > 0)
      list1 = list_prepend (list1, s);
  if (list1) {
    void cartesian_boundary_level (scalar * list, int l); 
    cartesian_boundary_level (list1, l);
    free (list1);
  }
}

#define realloc_scalar(size) realloc_scalar_gpu (size)

// fixme: for the moment only 'const int' are considered, this could be generalised
#define IS_EXTERNAL_CONSTANT(g) ((g)->constant && (g)->type == sym_INT && !(g)->data)
#define EXTERNAL_NAME(g) (g)->global == 2 ? "_loc_" : "", (g)->name, (g)->reduct ? "_in_" : ""

static char * str_append_array (char * dst, const char * list[])
{
  int empty = (dst == NULL);
  int len = empty ? 0 : strlen (dst);
  for (const char ** s = list; *s != NULL; s++)
    len += strlen (*s);
  dst = (char *) sysrealloc (dst, len + 1);
  if (empty) dst[0] = '\0';
  for (const char ** s = list; *s != NULL; s++)
    strcat (dst, *s);
  return dst;
}

#define str_append(dst, ...) str_append_array (dst, (const char *[]){__VA_ARGS__, NULL})
#define xstr(a) str(a)
#define str(a) #a
