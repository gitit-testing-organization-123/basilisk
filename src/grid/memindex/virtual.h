/**
# Multi-dimensional arrays using virtual memory

This implementation uses the Unix/Linux virtual memory subsystem to
more efficiently store sparse multidimensional arrays.

The `Memindex` structure defines multi-dimensional arrays. The
interface is that used by [/src/grid/tree.h](). */

#include <sys/mman.h>

struct _Memindex {
  int len;
#if dimension == 1
  char ** b;
  int len_x;
#elif dimension == 2  
  char *** b;
  int len_x;
  int len_y;
#else // dimension == 3
  char **** b;
  int len_x;
  int len_y;
  int len_z;
#endif
};

#define Memindex struct _Memindex *

/**
The `mem_data()` macros return the data stored at a specific
(multidimensional) index. It assumes that the index is allocated. This
can be checked with `mem_allocated()`. */

#if dimension == 1
// inline static
// const bool mem_allocated (const Memindex m, const int i) {
//   return (i >= 0 && i < m->len && m->b[i]);
// }
inline static 
const bool mem_allocated(const Memindex m, const int i) {
  return (i >= 0 && i < m->len_x && m->b[i]);
}
#define mem_data(m,i) ((m)->b[i])
#elif dimension == 2
// inline static
// const bool mem_allocated (const Memindex m, const int i, const int j) {
//   return (i >= 0 && i < m->len && m->b[i] &&
// 	  j >= 0 && j < m->len && m->b[i][j]);
// }
inline static 
const bool mem_allocated(const Memindex m, const int i, const int j) {
  return (i >= 0 && i < m->len_x && m->b[i] &&
	  j >= 0 && j < m->len_y && m->b[i][j]);
}
#define mem_data(m,i,j) ((m)->b[i][j])
#else // dimension == 3
// inline static
// const bool mem_allocated (const Memindex m, const int i, const int j, const int k) {
//   return (i >= 0 && i < m->len && m->b[i] &&
// 	  j >= 0 && j < m->len && m->b[i][j] &&
// 	  k >= 0 && k < m->len && m->b[i][j][k]);
// }
inline static
const bool mem_allocated (const Memindex m, const int i, const int j, const int k) {
  return (i >= 0 && i < m->len_x && m->b[i] &&
	  j >= 0 && j < m->len_y && m->b[i][j] &&
	  k >= 0 && k < m->len_z && m->b[i][j][k]);
}
#define mem_data(m,i,j,k) ((m)->b[i][j][k])
#endif // dimension == 3

/**
The `mem_new()` function returns a new (empty) `Memindex`. */

Memindex mem_new (int len)
{
  Memindex m = calloc (1, sizeof (struct _Memindex));
  return m;
}

/**
The `mem_destroy()` function frees all the memory allocated by a given
`Memindex`. */

void mem_destroy (Memindex m, int len)
{
#if dimension > 1
  for (int i = 0; i < m->len_x; i++)
    if (m->b[i]) {
  #if dimension > 2
      for (int j = 0; j < m->len_y; j++)
	if (m->b[i][j])
	  munmap (m->b[i][j], m->len_z*sizeof (char *));
  #endif // dimension > 2
      munmap (m->b[i], m->len_y*sizeof (char *));
    }
#endif // dimension > 1
  if (m->b)
    munmap (m->b, m->len_x*sizeof (char *));
  free (m);
}

/**
The `mem_assign()` function assigns a (pointer) value to a given index. */

#define mymap(len) mmap (NULL, len*sizeof (char *), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)

#if dimension == 1
// void mem_assign (Memindex m, int i, int len, void * b)
void mem_assign (Memindex m, int i, int len_x, void * b)
{
  if (!m->b) {
    // m->b = mymap (len);
    // m->len = len;
    m->b = mymap (len_x);
    m->len_x = len_x;
  }
  mem_data(m,i) = b;
}
#elif dimension == 2
// void mem_assign (Memindex m, int i, int j, int len, void * b)
void mem_assign (Memindex m, int i, int j, int len_x, int len_y, void * b)
{
  assert (b != NULL);
  if (!m->b) {
    // m->b = mymap (len);
    // m->len = len;
    m->b = mymap (len_x);
    m->len_x = len_x;
  }
  // if (!m->b[i]) 
  //   m->b[i] = mymap (len);
  if (!m->b[i]) {
    m->b[i] = mymap(len_y);
    m->len_y = len_y;
  }
  mem_data(m,i,j) = b;
}
#else // dimension == 3
// void mem_assign (Memindex m, int i, int j, int k, int len, void * b)
void mem_assign (Memindex m, int i, int j, int k, int len_x, int len_y, int len_z, void * b)
{
  if (!m->b) {
    // m->b = mymap (len);
    // m->len = len;
    m->b = mymap(len_x);
    m->len_x = len_x;
  }
  // if (!m->b[i])
  //   m->b[i] = mymap (len);
  if (!m->b[i]) {
    m->b[i] = mymap (len_y);
    m->len_y = len_y;
  }
  // if (!m->b[i][j])
  //   m->b[i][j] = mymap (len);  
  if (!m->b[i][j]) {
    m->b[i][j] = mymap(len_z);
    m->len_z = len_z;
  }
  mem_data(m,i,j,k) = b;
}
#endif // dimension == 3

/**
The `mem_free()` function frees a given index. */

static bool mem_free1d (char ** const b, const int i, int len)
{
 // Assumes 4k pages = 512*sizeof(char *) = 512*8 = 4096
  char ** const start = b + (i/512)*512;
  for (char ** j = start; j < start + min(len, 512); j++)
    if (*j)
      return false;
  if (madvise (start, 4096, MADV_DONTNEED))
    perror ("virtual.h: madvise: ");
  return true;
}

#if dimension == 1
// void mem_free (Memindex m, int i, int len, void * b)
void mem_free (Memindex m, int i, int len_x, void * b)
{  
  if (m->b[i]) {
    m->b[i] = NULL;
    mem_free1d (m->b, i, len_x);
  }
}
#elif dimension == 2
// void mem_free (Memindex m, int i, int j, int len, void * b)
void mem_free (Memindex m, int i, int j, int len_x, int len_y, void * b)
{
  if (m->b[i][j]) {
    m->b[i][j] = NULL;
    // if (mem_free1d (m->b[i], j, len))
    //   mem_free1d ((char **)m->b, i, len);
    if (mem_free1d (m->b[i], j, len_y))
      mem_free1d ((char **)m->b, i, len_x);
  }
}
#else // dimension == 3
// void mem_free (Memindex m, int i, int j, int k, int len, void * b)
void mem_free (Memindex m, int i, int j, int k, int len_x, int len_y, int len_z, void * b)
{
  if (m->b[i][j][k]) {
    m->b[i][j][k] = NULL;
  //   if (mem_free1d (m->b[i][j], k, len))
  //     if (mem_free1d ((char **)m->b[i], j, len))
	// mem_free1d ((char **)m->b, i, len);
    if (mem_free1d (m->b[i][j], k, len_z))
      if (mem_free1d ((char **)m->b[i], j, len_y))
	mem_free1d ((char **)m->b, i, len_x);
  }
}
#endif // dimension == 3

/**
The `foreach_mem()` macro traverses every `_i` allocated elements of
array `_m` taking into account a periodicity of `_len` (and ghost
cells). */

// macro foreach_mem (Memindex index, int len, int _i) {
//   Memindex _m = index;
//   int _len = len;
//   Point point = {0};
//   for (point.i = max(Period.x*GHOSTS, 0);
//        point.i < min(_len - Period.x*GHOSTS, _len);
//        point.i += _i)
//     if (_m->b[point.i])
// #if dimension > 1
//       for (point.j = max(Period.y*GHOSTS, 0);
// 	   point.j < min(_len - Period.y*GHOSTS, _len);
// 	   point.j += _i)
// 	if (_m->b[point.i][point.j])
// #if dimension > 2
// 	  for (point.k = max(Period.z*GHOSTS, 0);
// 	       point.k < min(_len - Period.z*GHOSTS, _len);
// 	       point.k += _i)
// 	    if (_m->b[point.i][point.j][point.k])
// #endif // dimension > 2
// #endif // dimension > 1
// 	      {...}
// }

#if dimension == 1
macro foreach_mem (Memindex index, int len_x, int _i) {
  Memindex _m = index;
  int _len_x = len_x;
  Point point = {0};
  for (point.i = max(Period.x*GHOSTS, 0); point.i < min(_len_x - Period.x*GHOSTS, _len_x); point.i += _i)
    if (_m->b[point.i])
	    {...}
}
#elif dimension == 2 
macro foreach_mem (Memindex index, int len_x, int len_y, int _i) {
  Memindex _m = index;
  int _len_x = len_x;
  int _len_y = len_y;
  Point point = {0};
  for (point.i = max(Period.x*GHOSTS, 0); point.i < min(_len_x - Period.x*GHOSTS, _len_x); point.i += _i)
    if (_m->b[point.i])
      for (point.j = max(Period.y*GHOSTS, 0); point.j < min(_len_y - Period.y*GHOSTS, _len_y); point.j += _i)
        if (_m->b[point.i][point.j])
	        {...}
}
#else // dimension == 3
macro foreach_mem (Memindex index, int len_x, int len_y, int len_z, int _i) {
  Memindex _m = index;
  int _len_x = len_x;
  int _len_y = len_y;
  int _len_z = len_z;
  Point point = {0};
  for (point.i = max(Period.x*GHOSTS, 0); point.i < min(_len_x - Period.x*GHOSTS, _len_x); point.i += _i)
    if (_m->b[point.i])
      for (point.j = max(Period.y*GHOSTS, 0); point.j < min(_len_y - Period.y*GHOSTS, _len_y); point.j += _i)
        if (_m->b[point.i][point.j])
	        for (point.k = max(Period.z*GHOSTS, 0); point.k < min(_len_z - Period.z*GHOSTS, _len_z); point.k += _i)
	          if (_m->b[point.i][point.j][point.k])
	            {...}
}
#endif 
