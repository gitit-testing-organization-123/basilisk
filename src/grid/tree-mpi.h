// this can be used to control the outputs of debug_mpi()
int debug_iteration = -1;

void debug_mpi (FILE * fp1);

typedef struct {
  CacheLevel * halo; // ghost cell indices for each level
  void * buf;        // MPI buffer
  MPI_Request r;     // MPI request
  int depth;         // the maximum number of levels
  int pid;           // the rank of the PE  
  int maxdepth;      // the maximum depth for this PE (= depth or depth + 1)
} Rcv;

typedef struct {
  Rcv * rcv;
  char * name;
  int npid;
} RcvPid;

typedef struct {
  RcvPid * rcv, * snd;
} SndRcv;

typedef struct {
  Boundary parent;
  
  SndRcv mpi_level, mpi_level_root, restriction;
  Array * send, * receive; // which pids do we send to/receive from
} MpiBoundary;

static int root_index (Point point);

static void cache_level_init (CacheLevel * c)
{
  c->p = NULL;
  c->n = c->nm = 0;
}

trace static bool cache_level_contains (CacheLevel * c, Point point)
{
  for (int i = 0; i < c->n; i++)
    if (c->p[i].i == point.i
#if dimension >= 2
	&& c->p[i].j == point.j
#endif
#if dimension >= 3
	&& c->p[i].k == point.k
#endif
	)
      return true;
  return false;
}

static void rcv_append (Point point, Rcv * rcv)
{
  bool forest = tree_number_of_roots() != 1;

  if (level > rcv->depth) {
    qrealloc (rcv->halo, level + 1, CacheLevel);
    for (int j = rcv->depth + 1; j <= level; j++)
      cache_level_init (&rcv->halo[j]);
    rcv->depth = level;
  }
  
  if (!forest || !cache_level_contains (&rcv->halo[level], point))
    cache_level_append (&rcv->halo[level], point);

  if (level > rcv->maxdepth)
    rcv->maxdepth = level;
}

void rcv_print (Rcv * rcv, FILE * fp, const char * prefix)
{
  for (int l = 0; l <= rcv->depth; l++)
    if (rcv->halo[l].n > 0)
      foreach_cache_level(rcv->halo[l], l)
	fprintf (fp, "%s%g %g %g %d %d\n", prefix, x, y, z, rcv->pid, level);
}

static void rcv_free_buf (Rcv * rcv)
{
  if (rcv->buf) {
    prof_start ("rcv_pid_receive");
    MPI_Wait (&rcv->r, MPI_STATUS_IGNORE);
    free (rcv->buf);
    rcv->buf = NULL;
    prof_stop();
  }
}

static void rcv_destroy (Rcv * rcv)
{
  rcv_free_buf (rcv);
  for (int i = 0; i <= rcv->depth; i++)
    if (rcv->halo[i].n > 0)
      free (rcv->halo[i].p);
  free (rcv->halo);
}

static RcvPid * rcv_pid_new (const char * name)
{
  RcvPid * r = qcalloc (1, RcvPid);
  r->name = strdup (name);
  return r;
}

static Rcv * rcv_pid_pointer (RcvPid * p, int pid)
{
  assert (pid >= 0 && pid < npe());
  
  int i;
  for (i = 0; i < p->npid; i++)
    if (pid == p->rcv[i].pid)
      break;

  if (i == p->npid) {
    qrealloc (p->rcv, ++p->npid, Rcv);
    Rcv * rcv = &p->rcv[p->npid-1];
    rcv->pid = pid;
    rcv->depth = rcv->maxdepth = 0;
    rcv->halo = qmalloc (1, CacheLevel);
    rcv->buf = NULL;
    cache_level_init (&rcv->halo[0]);
  }
  return &p->rcv[i];
}

static void rcv_pid_append (RcvPid * p, int pid, Point point)
{
  rcv_append (point, rcv_pid_pointer (p, pid));
}

static void rcv_pid_append_pids (RcvPid * p, Array * pids)
{
  // appends the pids of @p to @pids without duplication
  for (int i = 0; i < p->npid; i++) {
    int pid = p->rcv[i].pid, j, * a;
    for (j = 0, a = pids->p; j < pids->len/sizeof(int); j++,a++)
      if (*a == pid)
	break;
    if (j == pids->len/sizeof(int))
      array_append (pids, &pid, sizeof(int));
  }
}

void rcv_pid_write (RcvPid * p, const char * name)
{
  for (int i = 0; i < p->npid; i++) {
    Rcv * rcv = &p->rcv[i];
    char fname[80];
    sprintf (fname, "%s-%d-%d", name, pid(), rcv->pid);
    FILE * fp = fopen (fname, "w");
    rcv_print (rcv, fp, "");
    fclose (fp);
  }
}

static void rcv_pid_print (RcvPid * p, FILE * fp, const char * prefix)
{
  for (int i = 0; i < p->npid; i++)
    rcv_print (&p->rcv[i], fp, prefix);
}

static void rcv_pid_destroy (RcvPid * p)
{
  for (int i = 0; i < p->npid; i++)
    rcv_destroy (&p->rcv[i]);
  free (p->rcv);
  free (p->name);
  free (p);
}

static Boundary * mpi_boundary = NULL;

#define BOUNDARY_TAG(level) (level)
#define COARSEN_TAG(level)  ((level) + 64)
#define REFINE_TAG()        (128)
#define MOVED_TAG()         (256)
#define REQUEST_TAG()       (512)

void debug_mpi (FILE * fp1);

static void apply_bc (Rcv * rcv, scalar * list, scalar * listv,
		      vector * listf, int l, MPI_Status s)
{
  double * b = rcv->buf;
  foreach_cache_level(rcv->halo[l], l) {
    for (scalar s in list) {
      memcpy (&s[], b, sizeof(double)*s.block);
      b += s.block;
    }
    for (vector v in listf)
      foreach_dimension() {
	memcpy (&v.x[], b, sizeof(double)*v.x.block);
	b += v.x.block;
	if (*b != nodata && allocated(1))
	  memcpy (&v.x[1], b, sizeof(double)*v.x.block);
	b += v.x.block;
      }
    for (scalar s in listv) {
      for (int i = 0; i <= 1; i++)
	for (int j = 0; j <= 1; j++)
#if dimension == 3
	  for (int k = 0; k <= 1; k++) {
	    if (*b != nodata && allocated(i,j,k))
	      memcpy (&s[i,j,k], b, sizeof(double)*s.block);
	    b += s.block;
	  }
#else // dimension == 2
          {
	    if (*b != nodata && allocated(i,j))
	      memcpy (&s[i,j], b, sizeof(double)*s.block);
	    b += s.block;	    
          }
#endif // dimension == 2
    }
  }
  size_t size = b - (double *) rcv->buf;
  free (rcv->buf);
  rcv->buf = NULL;

  int rlen;
  MPI_Get_count (&s, MPI_DOUBLE, &rlen);
  if (rlen != size) {
    fprintf (stderr,
	     "rlen (%d) != size (%ld), %d receiving from %d at level %d\n"
	     "Calling debug_mpi(NULL)...\n"
	     "Aborting...\n",
	     rlen, size, pid(), rcv->pid, l);
    fflush (stderr);
    debug_mpi (NULL);
    MPI_Abort (MPI_COMM_WORLD, -2);
  }
}

#ifdef TIMEOUT
static struct {
  int count, source, tag;
  const char * name;
} RecvArgs;

static void timeout (int sig, siginfo_t *si, void *uc)
{
  fprintf (stderr,
	   "ERROR MPI_Recv \"%s\" (count = %d, source = %d, tag = %d):\n"
	   "Time out\n"
	   "Calling debug_mpi(NULL)...\n"
	   "Aborting...\n",
	   RecvArgs.name, RecvArgs.count, RecvArgs.source, RecvArgs.tag);
  fflush (stderr);
  debug_mpi (NULL);
  MPI_Abort (MPI_COMM_WORLD, -1);
}
#endif // TIMEOUT

static void mpi_recv_check (void * buf, int count, MPI_Datatype datatype,
			    int source, int tag,
			    MPI_Comm comm, MPI_Status * status,
			    const char * name)
{
#ifdef TIMEOUT
  RecvArgs.count = count;
  RecvArgs.source = source;
  RecvArgs.tag = tag;
  RecvArgs.name = name;
  
  timer_t timerid;
  extern double t;
  if (t > TIMEOUT) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timeout;
    sigemptyset(&sa.sa_mask);
    assert (sigaction(SIGRTMIN, &sa, NULL) != -1);

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    assert (timer_create(CLOCK_REALTIME, &sev, &timerid) != -1);

    struct itimerspec its;
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    assert (timer_settime(timerid, 0, &its, NULL) != -1);
  }
#endif // TIMEOUT
  
  int errorcode = MPI_Recv (buf, count, datatype, source, tag, comm, status);
  if (errorcode != MPI_SUCCESS) {
    char string[MPI_MAX_ERROR_STRING];
    int resultlen;
    MPI_Error_string (errorcode, string, &resultlen);
    fprintf (stderr,
	     "ERROR MPI_Recv \"%s\" (count = %d, source = %d, tag = %d):\n%s\n"
	     "Calling debug_mpi(NULL)...\n"
	     "Aborting...\n",
	     name, count, source, tag, string);
    fflush (stderr);
    debug_mpi (NULL);
    MPI_Abort (MPI_COMM_WORLD, -1);
  }

#ifdef TIMEOUT  
  if (t > TIMEOUT)
    assert (timer_delete (timerid) != -1);
#endif
}

trace
static int mpi_waitany (int count, MPI_Request array_of_requests[], int *indx,
			MPI_Status *status)
{
  return MPI_Waitany (count, array_of_requests, indx, status);
}

static int list_lenb (scalar * list) {
  int len = 0;
  for (scalar s in list)
    len += s.block;
  return len;
}

static int vectors_lenb (vector * list) {
  int len = 0;
  for (vector v in list)
    len += v.x.block;
  return len;
}

static void rcv_pid_receive (RcvPid * m, scalar * list, scalar * listv,
			     vector * listf, int l)
{
  if (m->npid == 0)
    return;
  
  prof_start ("rcv_pid_receive");

  int len = list_lenb (list) + 2*dimension*vectors_lenb (listf) +
    (1 << dimension)*list_lenb (listv);

  MPI_Request r[m->npid];
  Rcv * rrcv[m->npid]; // fixme: using NULL requests should be OK
  int nr = 0;
  for (int i = 0; i < m->npid; i++) {
    Rcv * rcv = &m->rcv[i];
    if (l <= rcv->depth && rcv->halo[l].n > 0) {
      assert (!rcv->buf);
      rcv->buf = malloc (sizeof (double)*rcv->halo[l].n*len);
#if 0
      fprintf (stderr, "%s receiving %d doubles from %d level %d\n",
	       m->name, rcv->halo[l].n*len, rcv->pid, l);
      fflush (stderr);
#endif
#if 1 /* initiate non-blocking receive */
      MPI_Irecv (rcv->buf, rcv->halo[l].n*len, MPI_DOUBLE, rcv->pid,
		 BOUNDARY_TAG(l), MPI_COMM_WORLD, &r[nr]);
      rrcv[nr++] = rcv;
#else /* blocking receive (useful for debugging) */
      MPI_Status s;
      mpi_recv_check (rcv->buf, rcv->halo[l].n*len, MPI_DOUBLE, rcv->pid,
		      BOUNDARY_TAG(l), MPI_COMM_WORLD, &s, "rcv_pid_receive");
      apply_bc (rcv, list, listf, listv, l, s);
#endif
    }
  }

  /* non-blocking receives (does nothing when using blocking receives) */
  if (nr > 0) {
    int i;
    MPI_Status s;
    mpi_waitany (nr, r, &i, &s);
    while (i != MPI_UNDEFINED) {
      Rcv * rcv = rrcv[i];
      assert (l <= rcv->depth && rcv->halo[l].n > 0);
      assert (rcv->buf);
      apply_bc (rcv, list, listv, listf, l, s);
      mpi_waitany (nr, r, &i, &s);
    }
  }
  
  prof_stop();
}

trace
static void rcv_pid_wait (RcvPid * m)
{
  /* wait for completion of send requests */
  for (int i = 0; i < m->npid; i++)
    rcv_free_buf (&m->rcv[i]);
}

static void rcv_pid_send (RcvPid * m, scalar * list, scalar * listv,
			  vector * listf, int l)
{
  if (m->npid == 0)
    return;

  prof_start ("rcv_pid_send");

  int len = list_lenb (list) + 2*dimension*vectors_lenb (listf) +
    (1 << dimension)*list_lenb (listv);

  /* send ghost values */
  for (int i = 0; i < m->npid; i++) {
    Rcv * rcv = &m->rcv[i];
    if (l <= rcv->depth && rcv->halo[l].n > 0) {
      assert (!rcv->buf);
      rcv->buf = malloc (sizeof (double)*rcv->halo[l].n*len);
      double * b = rcv->buf;
      foreach_cache_level(rcv->halo[l], l) {
	for (scalar s in list) {
	  memcpy (b, &s[], sizeof(double)*s.block);
	  b += s.block;
	}
	for (vector v in listf)
	  foreach_dimension() {
	    memcpy (b, &v.x[], sizeof(double)*v.x.block);
	    b += v.x.block;
	    if (allocated(1))
	      memcpy (b, &v.x[1], sizeof(double)*v.x.block);
	    else
	      *b = nodata;
	    b += v.x.block;
	  }
	for (scalar s in listv) {
	  for (int i = 0; i <= 1; i++)
	    for (int j = 0; j <= 1; j++)
#if dimension == 3
	      for (int k = 0; k <= 1; k++) {
		if (allocated(i,j,k))
		  memcpy (b, &s[i,j,k], sizeof(double)*s.block);
		else
		  *b = nodata;
		b += s.block;
	      }
#else // dimension == 2
	      {
		if (allocated(i,j))
		  memcpy (b, &s[i,j], sizeof(double)*s.block);
		else
		  *b = nodata;
		b += s.block;
	      }
#endif // dimension == 2
	}
      }
#if 0
      fprintf (stderr, "%s sending %d doubles to %d level %d\n",
	       m->name, rcv->halo[l].n*len, rcv->pid, l);
      fflush (stderr);
#endif
      MPI_Isend (rcv->buf, (b - (double *) rcv->buf),
		 MPI_DOUBLE, rcv->pid, BOUNDARY_TAG(l), MPI_COMM_WORLD,
		 &rcv->r);
    }
  }

  prof_stop();
}

static void rcv_pid_sync (SndRcv * m, scalar * list, int l)
{
  scalar * listr = NULL, * listv = NULL;
  vector * listf = NULL;
  for (scalar s in list)
    if (!is_constant(s) && s.block > 0) {
      if (s.face)
	listf = vectors_add (listf, s.v);
      else if (s.restriction == restriction_vertex)
	listv = list_add (listv, s);
      else
	listr = list_add (listr, s);
    }
  rcv_pid_send (m->snd, listr, listv, listf, l);
  rcv_pid_receive (m->rcv, listr, listv, listf, l);
  rcv_pid_wait (m->snd);
  free (listr);
  free (listf);
  free (listv);
}

static void snd_rcv_destroy (SndRcv * m)
{
  rcv_pid_destroy (m->rcv);
  rcv_pid_destroy (m->snd);
}

static void snd_rcv_init (SndRcv * m, const char * name)
{
  char s[strlen(name) + 5];
  strcpy (s, name);
  strcat (s, ".rcv");
  m->rcv = rcv_pid_new (s);
  strcpy (s, name);
  strcat (s, ".snd");
  m->snd = rcv_pid_new (s);
}

static void mpi_boundary_destroy (Boundary * b)
{
  MpiBoundary * m = (MpiBoundary *) b;
  snd_rcv_destroy (&m->mpi_level);
  snd_rcv_destroy (&m->mpi_level_root);
  snd_rcv_destroy (&m->restriction);
  array_free (m->send);
  array_free (m->receive);
  free (m);
}

trace
static void mpi_boundary_level (const Boundary * b, scalar * list, int l)
{
  MpiBoundary * m = (MpiBoundary *) b;
  rcv_pid_sync (&m->mpi_level, list, l);
  rcv_pid_sync (&m->mpi_level_root, list, l);
}

trace
static void mpi_boundary_restriction (const Boundary * b, scalar * list, int l)
{
  MpiBoundary * m = (MpiBoundary *) b;
  rcv_pid_sync (&m->restriction, list, l);
}

void mpi_boundary_new()
{
  mpi_boundary = (Boundary *) qcalloc (1, MpiBoundary);
  mpi_boundary->destroy = mpi_boundary_destroy;
  mpi_boundary->level = mpi_boundary_level;
  mpi_boundary->restriction = mpi_boundary_restriction;
  MpiBoundary * mpi = (MpiBoundary *) mpi_boundary;
  snd_rcv_init (&mpi->mpi_level, "mpi_level");
  snd_rcv_init (&mpi->mpi_level_root, "mpi_level_root");
  snd_rcv_init (&mpi->restriction, "restriction");
  mpi->send = array_new();
  mpi->receive = array_new();
  add_boundary (mpi_boundary);
}

static FILE * fopen_prefix (FILE * fp, const char * name, char * prefix)
{
  if (fp) {
    sprintf (prefix, "%s-%d ", name, pid());
    return fp;
  }
  else {
    strcpy (prefix, "");
    char fname[80];
    if (debug_iteration >= 0)
      sprintf (fname, "%s-%d-%d", name, debug_iteration, pid());
    else
      sprintf (fname, "%s-%d", name, pid());
    return fopen (fname, "w");
  }
}

void debug_mpi (FILE * fp1)
{
  void output_cells_internal (FILE * fp);

  char prefix[80];
  FILE * fp;

  // cleanup
  if (fp1 == NULL) {
    char name[80];
    sprintf (name, "halo-%d", pid()); remove (name);
    sprintf (name, "cells-%d", pid()); remove (name);
    sprintf (name, "faces-%d", pid()); remove (name);
    sprintf (name, "vertices-%d", pid()); remove (name);
    sprintf (name, "neighbors-%d", pid()); remove (name);
    sprintf (name, "mpi-level-rcv-%d", pid()); remove (name);
    sprintf (name, "mpi-level-snd-%d", pid()); remove (name);
    sprintf (name, "mpi-level-root-rcv-%d", pid()); remove (name);
    sprintf (name, "mpi-level-root-snd-%d", pid()); remove (name);
    sprintf (name, "mpi-restriction-rcv-%d", pid()); remove (name);
    sprintf (name, "mpi-restriction-snd-%d", pid()); remove (name);
    sprintf (name, "mpi-border-%d", pid()); remove (name);
    sprintf (name, "exterior-%d", pid()); remove (name);
    sprintf (name, "depth-%d", pid()); remove (name);
    sprintf (name, "refined-%d", pid()); remove (name);
  }
  
  // local halo
  fp = fopen_prefix (fp1, "halo", prefix);
  for (int l = 0; l < depth(); l++)
    foreach_halo (prolongation, l)
      foreach_child()
        fprintf (fp, "%s%g %g %g %d\n", prefix, x, y, z, level);
  if (!fp1)
    fclose (fp);

  if (!fp1) {
    fp = fopen_prefix (fp1, "cells", prefix);
    output_cells_internal (fp);
    fclose (fp);
  }
  
  fp = fopen_prefix (fp1, "faces", prefix);
  foreach_face()
    fprintf (fp, "%s%g %g %g %d\n", prefix, x, y, z, level);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "vertices", prefix);
  foreach_vertex()
    fprintf (fp, "%s%g %g %g %d\n", prefix, x, y, z, level);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "neighbors", prefix);
  foreach() {
    int n = 0;
    foreach_neighbor(1)
      if (is_refined(cell))
	n++;
    fprintf (fp, "%s%g %g %g %d\n", prefix, x, y, z, cell.neighbors);
    assert (cell.neighbors == n);
  }
  if (!fp1)
    fclose (fp);

  MpiBoundary * mpi = (MpiBoundary *) mpi_boundary;
  
  fp = fopen_prefix (fp1, "mpi-level-rcv", prefix);
  rcv_pid_print (mpi->mpi_level.rcv, fp, prefix);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "mpi-level-root-rcv", prefix);
  rcv_pid_print (mpi->mpi_level_root.rcv, fp, prefix);
  if (!fp1)
    fclose (fp);
    
  fp = fopen_prefix (fp1, "mpi-restriction-rcv", prefix);
  rcv_pid_print (mpi->restriction.rcv, fp, prefix);
  if (!fp1)
    fclose (fp);
    
  fp = fopen_prefix (fp1, "mpi-level-snd", prefix);
  rcv_pid_print (mpi->mpi_level.snd, fp, prefix);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "mpi-level-root-snd", prefix);
  rcv_pid_print (mpi->mpi_level_root.snd, fp, prefix);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "mpi-restriction-snd", prefix);
  rcv_pid_print (mpi->restriction.snd, fp, prefix);
  if (!fp1)
    fclose (fp);
    
  fp = fopen_prefix (fp1, "mpi-border", prefix);
  foreach_cell() {
    if (is_border(cell))
      fprintf (fp, "%s%g %g %g %d %d %d\n",
	       prefix, x, y, z, level, cell.neighbors, cell.pid);
    else
      continue;
    if (is_leaf(cell))
      continue;
  }
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "exterior", prefix);
  foreach_cell() {
    if (!is_local(cell))
      fprintf (fp, "%s%g %g %g %d %d %d %d\n",
	       prefix, x, y, z, level, cell.neighbors,
	       cell.pid, cell.flags & leaf);
#if 0
    else if (is_active(cell) && !is_border(cell))
      continue;
    if (is_leaf(cell))
      continue;
#endif
  }
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "depth", prefix);
  fprintf (fp, "depth: %d %d\n", pid(), depth());
  fprintf (fp, "======= mpi_level.snd ======\n");
  RcvPid * snd = mpi->mpi_level.snd;
  for (int i = 0; i < snd->npid; i++)
    fprintf (fp, "%d %d %d\n", pid(), snd->rcv[i].pid, snd->rcv[i].maxdepth);
  fprintf (fp, "======= mpi_level.rcv ======\n");
  snd = mpi->mpi_level.rcv;
  for (int i = 0; i < snd->npid; i++)
    fprintf (fp, "%d %d %d\n", pid(), snd->rcv[i].pid, snd->rcv[i].maxdepth);
  if (!fp1)
    fclose (fp);

  fp = fopen_prefix (fp1, "refined", prefix);
  foreach_cache (tree->refined)
    fprintf (fp, "%s%g %g %g %d\n", prefix, x, y, z, level);
  if (!fp1)
    fclose (fp);
}

static void snd_rcv_free (SndRcv * p)
{
  char name[strlen(p->rcv->name) + 1];
  strcpy (name, p->rcv->name);
  rcv_pid_destroy (p->rcv);
  p->rcv = rcv_pid_new (name);
  strcpy (name, p->snd->name);
  rcv_pid_destroy (p->snd);
  p->snd = rcv_pid_new (name);
}

static bool is_root (Point point)
{
  if (is_refined(cell))
    foreach_child()
      if (is_local(cell))
	return true;
  return false;
}

// see src/figures/prolongation.svg
static bool is_local_prolongation (Point point, Point p)
{
#if dimension == 2
  struct { int x, y; } dp = {p.i - point.i, p.j - point.j};
#elif dimension == 3
  struct { int x, y, z; } dp = {p.i - point.i, p.j - point.j, p.k - point.k};
#endif
  foreach_dimension() {
    if (dp.x == 0 && (is_refined(neighbor(-1)) || is_refined(neighbor(1))))
      return true;
    if (is_refined(neighbor(dp.x)))
      return true;
  }
  return false;
}

#define is_remote(cell) (cell.pid >= 0 && cell.pid != pid())

static void append_pid (Array * pids, int pid)
{
  for (int i = 0, * p = (int *) pids->p; i < pids->len/sizeof(int); i++, p++)
    if (*p == pid)
      return;
  array_append (pids, &pid, sizeof(int));
}

static int locals_pids (Point point, Array * pids)
{
  if (is_leaf(cell)) { // prolongation
    if (is_local(cell)) {
      Point p = point;
      foreach_neighbor(1) {
	if (is_remote(cell) &&
	    (is_refined(cell) || is_local_prolongation (point, p)))
	  append_pid (pids, cell.pid);
	if (is_refined(cell))
	  foreach_child()
	    if (is_remote(cell))
	      append_pid (pids, cell.pid);
      }
    }
  }
  else
    foreach_neighbor(1) {
      if (is_remote(cell))
	append_pid (pids, cell.pid);
      if (is_refined(cell))
	foreach_child()
	  if (is_remote(cell))
	    append_pid (pids, cell.pid);
    }
  return pids->len/sizeof(int);
}

static int root_pids (Point point, Array * pids)
{
  foreach_child()
    if (is_remote(cell))
      append_pid (pids, cell.pid);
  return pids->len/sizeof(int);
}

static bool has_local_child (Point point);

static bool root_has_local_data (Point point)
{
  if (is_local(cell))
    return true;
  return cell.neighbors && has_local_child (point);
}

static void request_level0_root_neighbors (Point point,
					   SndRcv * mpi_level_root,
					   SndRcv * restriction,
					   unsigned short used)
{
  if (level != 0 || !root_has_local_data (point))
    return;

  /* Forest roots have real same-level neighbors.  A level-1 child can need
     either its remote parent root or an adjacent remote root as a coarse
     prolongation stencil, before the usual old sender-side scan sees it. */
  Point root = point;
  if (is_remote(cell)) {
    rcv_pid_append (mpi_level_root->rcv, cell.pid, root);
    rcv_pid_append (restriction->rcv, cell.pid, root);
    cell.flags |= used;
  }

  foreach_neighbor(1) {
    if (!allocated(0))
      continue;
    if (point.i == root.i
#if dimension >= 2
	&& point.j == root.j
#endif
#if dimension >= 3
	&& point.k == root.k
#endif
	)
      continue;
    if (is_remote(cell)) {
      rcv_pid_append (mpi_level_root->rcv, cell.pid, point);
      rcv_pid_append (restriction->rcv, cell.pid, point);
      cell.flags |= used;
    }
  }
}

static int coarse_request_count (Rcv * rcv, int maxlevel)
{
  int depth = min (maxlevel, rcv->depth);
  int n = 1;
  bool non_empty = false;
  for (int l = 0; l <= depth; l++) {
    int len = rcv->halo[l].n;
    n++;
    if (len > 0) {
      non_empty = true;
      n += dimension*len;
    }
  }
  return non_empty ? n : 0;
}

static int coarse_request_pack (Rcv * rcv, int * buf, int maxlevel)
{
  int * start = buf;
  int depth = min (maxlevel, rcv->depth);
  *buf++ = depth;
  for (int l = 0; l <= depth; l++) {
    CacheLevel * halo = &rcv->halo[l];
    *buf++ = halo->n;
    for (int i = 0; i < halo->n; i++) {
      *buf++ = halo->p[i].i;
#if dimension >= 2
      *buf++ = halo->p[i].j;
#endif
#if dimension >= 3
      *buf++ = halo->p[i].k;
#endif
    }
  }
  return buf - start;
}

static void coarse_request_unpack (RcvPid * snd, int requester, int * buf,
				   int count)
{
  int * start = buf;
  int depth = *buf++;
  for (int l = 0; l <= depth; l++) {
    int n = *buf++;
    for (int i = 0; i < n; i++) {
      Point point = {0};
      point.level = l;
      point.i = *buf++;
#if dimension >= 2
      point.j = *buf++;
#endif
#if dimension >= 3
      point.k = *buf++;
#endif
      rcv_pid_append (snd, requester, point);
    }
  }
  assert (buf - start == count);
}

static void rcv_clear_coarse (Rcv * rcv, int maxlevel)
{
  int depth = min (maxlevel, rcv->depth);
  for (int l = 0; l <= depth; l++) {
    free (rcv->halo[l].p);
    cache_level_init (&rcv->halo[l]);
  }
}

static void rcv_pid_clear_coarse (RcvPid * p, int maxlevel)
{
  for (int i = 0; i < p->npid; i++)
    rcv_clear_coarse (&p->rcv[i], maxlevel);
}

static void rcv_filter_level0_owner (Rcv * rcv)
{
  if (rcv->depth < 0 || rcv->halo[0].n == 0)
    return;

  CacheLevel * halo = &rcv->halo[0];
  int n = 0;
  for (int i = 0; i < halo->n; i++) {
    Point point = {0};
    point.level = 0;
    point.i = halo->p[i].i;
#if dimension >= 2
    point.j = halo->p[i].j;
#endif
#if dimension >= 3
    point.k = halo->p[i].k;
#endif
    if (allocated(0) && cell.pid >= 0 && cell.pid != rcv->pid)
      continue;
    halo->p[n++] = halo->p[i];
  }
  halo->n = n;
}

static void rcv_pid_filter_level0_owners (RcvPid * p)
{
  for (int i = 0; i < p->npid; i++)
    rcv_filter_level0_owner (&p->rcv[i]);
}

/* Replace only coarse points in the old sender-side lists with the exact
   points requested by remote ranks.  Finer levels remain controlled by the old
   sender-side scan. */
static void snd_coarse_from_rcv_requests (SndRcv * m, int maxlevel)
{
  int np = npe();
  int * sendcounts = qcalloc (np, int);
  int * recvcounts = qcalloc (np, int);

  for (int i = 0; i < m->rcv->npid; i++) {
    Rcv * rcv = &m->rcv->rcv[i];
    sendcounts[rcv->pid] = coarse_request_count (rcv, maxlevel);
  }

  MPI_Alltoall (sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT,
		MPI_COMM_WORLD);

  rcv_pid_clear_coarse (m->snd, maxlevel);

  int ** sendbufs = qcalloc (np, int *);
  int ** recvbufs = qcalloc (np, int *);
  MPI_Request * requests = qmalloc (2*np, MPI_Request);
  int nr = 0;

  for (int p = 0; p < np; p++)
    if (recvcounts[p] > 0) {
      recvbufs[p] = qmalloc (recvcounts[p], int);
      MPI_Irecv (recvbufs[p], recvcounts[p], MPI_INT, p, REQUEST_TAG(),
		 MPI_COMM_WORLD, &requests[nr++]);
    }

  for (int i = 0; i < m->rcv->npid; i++) {
    Rcv * rcv = &m->rcv->rcv[i];
    int dest = rcv->pid;
    if (sendcounts[dest] > 0) {
      sendbufs[dest] = qmalloc (sendcounts[dest], int);
      int n = coarse_request_pack (rcv, sendbufs[dest], maxlevel);
      assert (n == sendcounts[dest]);
      MPI_Isend (sendbufs[dest], sendcounts[dest], MPI_INT, dest,
		 REQUEST_TAG(), MPI_COMM_WORLD, &requests[nr++]);
    }
  }

  if (nr > 0)
    MPI_Waitall (nr, requests, MPI_STATUSES_IGNORE);

  for (int p = 0; p < np; p++) {
    if (recvcounts[p] > 0)
      coarse_request_unpack (m->snd, p, recvbufs[p], recvcounts[p]);
    free (sendbufs[p]);
    free (recvbufs[p]);
  }

  free (requests);
  free (sendbufs);
  free (recvbufs);
  free (sendcounts);
  free (recvcounts);
}

static void sync_level0_root_pids()
{
  int numroots = tree_number_of_roots();
  if (numroots <= 1)
    return;

  int * owners = qcalloc (numroots, int);

  foreach_cell_all() {
    if (level == 0) {
      int r = root_index (point);
      if (r >= 0 && r < numroots && is_local(cell))
	owners[r] = pid() + 1;
      continue;
    }
  }

  mpi_all_reduce_array (owners, MPI_INT, MPI_MAX, numroots);

  foreach_cell_all() {
    if (level == 0) {
      int r = root_index (point);
      if (r >= 0 && r < numroots && owners[r] > 0 && cell.pid >= 0)
	cell.pid = owners[r] - 1;
      continue;
    }
  }

  free (owners);
}

// turns on tree_check() and co with outputs controlled by the condition
// #define DEBUGCOND (pid() >= 300 && pid() <= 400 && t > 0.0784876)

// turns on tree_check() and co without any outputs
// #define DEBUGCOND false

static void rcv_pid_row (RcvPid * m, int l, int * row)
{
  for (int i = 0; i < npe(); i++)
    row[i] = 0;
  for (int i = 0; i < m->npid; i++) {
    Rcv * rcv = &m->rcv[i];
    if (l <= rcv->depth && rcv->halo[l].n > 0)
      row[rcv->pid] = rcv->halo[l].n;
  }
}

void check_snd_rcv_matrix (SndRcv * sndrcv, const char * name)
{
  int maxlevel = depth();
  mpi_all_reduce (maxlevel, MPI_INT, MPI_MAX);
  int * row = qmalloc (npe(), int);
  for (int l = 0; l <= maxlevel; l++) {
    int status = 0;
    if (pid() == 0) { // master
      // send/receive matrix i.e.
      // send[i][j] = number of points sent/received from i to j
      int ** send = matrix_new (npe(), npe(), sizeof(int));
      int ** receive = matrix_new (npe(), npe(), sizeof(int));
      rcv_pid_row (sndrcv->snd, l, row);
      MPI_Gather (row, npe(), MPI_INT, &send[0][0], npe(), MPI_INT, 0,
		  MPI_COMM_WORLD); 
      rcv_pid_row (sndrcv->rcv, l, row);
      MPI_Gather (row, npe(), MPI_INT, &receive[0][0], npe(), MPI_INT, 0,
		  MPI_COMM_WORLD);

      int * astatus = qmalloc (npe(), int);
      for (int i = 0; i < npe(); i++)
	astatus[i] = 0;
      for (int i = 0; i < npe(); i++)
	for (int j = 0; j < npe(); j++)
	  if (send[i][j] != receive[j][i]) {
	    fprintf (stderr, "%s: %d sends    %d to   %d at level %d\n",
		     name, i, send[i][j], j, l);
	    fprintf (stderr, "%s: %d receives %d from %d at level %d\n",
		     name, j, receive[j][i], i, l);
	    fflush (stderr);
	    for (int k = i - 2; k <= i + 2; k++)
	      if (k >= 0 && k < npe())
		astatus[k] = 1;
	    for (int k = j - 2; k <= j + 2; k++)
	      if (k >= 0 && k < npe())
		astatus[k] = 1;
	  }
      MPI_Scatter (astatus, 1, MPI_INT, &status, 1, MPI_INT, 0, MPI_COMM_WORLD);
      free (astatus);

      matrix_free (send);
      matrix_free (receive);
    }
    else { // slave
      rcv_pid_row (sndrcv->snd, l, row);
      MPI_Gather (row, npe(), MPI_INT, NULL, npe(), MPI_INT, 0, MPI_COMM_WORLD); 
      rcv_pid_row (sndrcv->rcv, l, row);
      MPI_Gather (row, npe(), MPI_INT, NULL, npe(), MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Scatter (NULL, 1, MPI_INT, &status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }
    if (status) {
      fprintf (stderr,
	       "check_snd_rcv_matrix \"%s\" failed\n"
	       "Calling debug_mpi(NULL)...\n"
	       "Aborting...\n",
	       name);
      fflush (stderr);
      debug_mpi (NULL);
      MPI_Abort (MPI_COMM_WORLD, -3);
    }
  }
  free (row);
}

static bool has_local_child (Point point)
{
  foreach_child()
    if (is_local(cell))
      return true;
  return false;
}

trace
void mpi_boundary_update_buffers()
{
  if (npe() == 1)
    return;

  prof_start ("mpi_boundary_update_buffers");

  MpiBoundary * m = (MpiBoundary *) mpi_boundary;
  SndRcv * mpi_level = &m->mpi_level;
  SndRcv * mpi_level_root = &m->mpi_level_root;
  SndRcv * restriction = &m->restriction;

  snd_rcv_free (mpi_level);
  snd_rcv_free (mpi_level_root);
  snd_rcv_free (restriction);
  
  static const unsigned short used = 1 << user;

  bool forest = tree_number_of_roots () != 1;

  if (forest) {
    sync_level0_root_pids();
    foreach_cell() {
      request_level0_root_neighbors (point, mpi_level_root, restriction, used);
      if (is_leaf(cell))
        continue;
    }
  } 

  foreach_cell() {
    if (is_active(cell) && !is_border(cell))
      /* We skip the interior of the local domain.
	 Note that this can be commented out in case of suspicion that
	 something is wrong with border cell tagging. */
      continue;
    
    if (cell.neighbors) {
      // sending
      Array pids = {NULL, 0, 0};
      int n = locals_pids (point, &pids);
      if (n) {
	foreach_child()
	  if (is_local(cell))
	    for (int i = 0, * p = (int *) pids.p; i < n; i++, p++)
	      rcv_pid_append (mpi_level->snd, *p, point);
	free (pids.p);
      }
      // receiving
      bool locals = false;
      if (is_leaf(cell)) { // prolongation
	if (is_remote(cell)) {
	  Point p = point;
	  foreach_neighbor(1)
	    if ((is_local(cell) &&
		 (is_refined(cell) || is_local_prolongation (point, p))) ||
		is_root(point)) {
	      locals = true; break;
	    }
	}
      }
      else
	foreach_neighbor(1)
	  if (is_local(cell) || is_root(point)) {
	    locals = true; break;
	  }
      if (locals)
	foreach_child()
	  if (is_remote(cell))
            rcv_pid_append (mpi_level->rcv, cell.pid, point),
	      cell.flags |= used;
      
      // root cells
      if (!is_leaf(cell)) {
	// sending
	if (is_local(cell)) {
	  Array pids = {NULL, 0, 0};
	  // root cell
	  int n = root_pids (point, &pids);
	  if (n) {
	    foreach_neighbor()
	      for (int i = 0, * p = (int *) pids.p; i < n; i++, p++)
		if (cell.pid >= 0 && cell.pid != *p)
		  rcv_pid_append (mpi_level_root->snd, *p, point);
	    // restriction (remote root)
	    for (int i = 0, * p = (int *) pids.p; i < n; i++, p++)
	      rcv_pid_append (restriction->snd, *p, point);
	    free (pids.p);
	  }
	}
	// receiving
	else if (is_remote(cell)) {
	  bool root = false;
	  foreach_child()
	    if (is_local(cell)) {
	      root = true; break;
	    }
	  if (root) {
	    int pid = cell.pid;
	    foreach_neighbor()
	      if (is_remote(cell))
		rcv_pid_append (mpi_level_root->rcv, pid, point),
		  cell.flags |= used;
	    // restriction (remote root)
	    rcv_pid_append (restriction->rcv, pid, point);
	  }
	}
      }
    }

    // restriction (remote siblings/children)
    if (level > 0) {
      if (is_local(cell)) {
	// sending
	Array pids = {NULL, 0, 0};
	if (is_remote(aparent(0)))
	  append_pid (&pids, aparent(0).pid);
	int n = root_pids (parent, &pids);
	if (n) {
	  for (int i = 0, * p = (int *) pids.p; i < n; i++, p++)
	    rcv_pid_append (restriction->snd, *p, point);
	  free (pids.p);
	}
      }
      else if (is_remote(cell)) {
	// receiving
	if (is_local(aparent(0)) || has_local_child (parent))
	  rcv_pid_append (restriction->rcv, cell.pid, point);
      }
    }
  }
    
  /* we remove unused cells
     we do a breadth-first traversal from fine to coarse, so that
     coarsening of unused cells can proceed fully. */
  
  static const unsigned short keep = 1 << (user + 1);
  for (int l = depth(); l >= 0; l--)
    foreach_cell()
      if (level == l) {
	if (level > 0 && (cell.pid < 0 || is_local(cell) || (cell.flags & used)))
	  aparent(0).flags |= keep;
	if (is_refined(cell) && !(cell.flags & keep) &&
	    !(level == 0 && (cell.flags & used)))
	  coarsen_cell (point, NULL);
	cell.flags &= ~(used|keep);
	continue; // level == l
      }

  if (forest) {
    rcv_pid_filter_level0_owners (mpi_level->rcv);
    rcv_pid_filter_level0_owners (mpi_level_root->rcv);
    rcv_pid_filter_level0_owners (restriction->rcv);

    snd_coarse_from_rcv_requests (mpi_level, 1);
    snd_coarse_from_rcv_requests (mpi_level_root, 0);
    snd_coarse_from_rcv_requests (restriction, 0);
  }

  /* we update the list of send/receive pids */
  m->send->len = m->receive->len = 0;
  rcv_pid_append_pids (mpi_level->snd, m->send);
  rcv_pid_append_pids (mpi_level_root->snd, m->send);
  rcv_pid_append_pids (mpi_level->rcv, m->receive);
  rcv_pid_append_pids (mpi_level_root->rcv, m->receive);
  
  prof_stop();

#if DEBUG_MPI
  debug_mpi (NULL);
#endif

#ifdef DEBUGCOND
  extern double t; NOT_UNUSED(t);
  check_snd_rcv_matrix (mpi_level, "mpi_level");
  check_snd_rcv_matrix (mpi_level_root, "mpi_level_root");
  check_snd_rcv_matrix (restriction, "restriction");
  if (DEBUGCOND)
    debug_mpi (NULL);
  tree_check();
#endif
}

trace
void mpi_boundary_refine (scalar * list)
{
  prof_start ("mpi_boundary_refine");

  MpiBoundary * mpi = (MpiBoundary *) mpi_boundary;

  /* Send refinement cache to each neighboring process. */
  Array * snd = mpi->send;
  MPI_Request r[2*snd->len/sizeof(int)];
  int nr = 0;
  for (int i = 0, * dest = snd->p; i < snd->len/sizeof(int); i++,dest++) {
    int len = tree->refined.n;
    MPI_Isend (&tree->refined.n, 1, MPI_INT, *dest,
	       REFINE_TAG(), MPI_COMM_WORLD, &r[nr++]);
    if (len > 0)
      MPI_Isend (tree->refined.p, sizeof(Index)/sizeof(int)*len,
		 MPI_INT, *dest, REFINE_TAG(), MPI_COMM_WORLD, &r[nr++]);
  }

  /* Receive refinement cache from each neighboring process. 
   fixme: use non-blocking receives */
  Array * rcv = mpi->receive;
  Cache rerefined = {NULL, 0, 0};
  for (int i = 0, * source = rcv->p; i < rcv->len/sizeof(int); i++,source++) {
    int len;
    mpi_recv_check (&len, 1, MPI_INT, *source, REFINE_TAG(),
		    MPI_COMM_WORLD, MPI_STATUS_IGNORE,
		    "mpi_boundary_refine (len)");
    if (len > 0) {
      Index p[len];
      mpi_recv_check (p, sizeof(Index)/sizeof(int)*len,
		      MPI_INT, *source, REFINE_TAG(),
		      MPI_COMM_WORLD, MPI_STATUS_IGNORE,
		      "mpi_boundary_refine (p)");
      Cache refined = {p, len, len};
      foreach_cache (refined)
	if (level <= depth() && allocated(0)) {
	  if (is_leaf(cell)) {
	    bool neighbors = false;
	    foreach_neighbor()
	      if (allocated(0) && (is_active(cell) || is_local(aparent(0)))) {
		neighbors = true; break;
	      }
	    // refine the cell only if it has local neighbors
	    if (neighbors)
	      refine_cell (point, list, 0, &rerefined);
	  }
	}
    }
  }

  /* check that caches were received OK and free ressources */
  if (nr)
    MPI_Waitall (nr, r, MPI_STATUSES_IGNORE);

  /* update the refinement cache with "re-refined" cells */
  free (tree->refined.p);
  tree->refined = rerefined;
  
  prof_stop();

  /* if any cell has been re-refined, we repeat the process to take care
     of recursive refinements induced by the 2:1 constraint */
  mpi_all_reduce (rerefined.n, MPI_INT, MPI_SUM);
  if (rerefined.n)
    mpi_boundary_refine (list);
  for (scalar s in list)
    s.dirty = true;
}

static void check_depth()
{
#if DEBUG_MPI 
  int max = 0;
  foreach_cell_all()
    if (level > max)
      max = level;
  if (depth() != max) {
    FILE * fp = fopen ("layer", "w");
    fprintf (fp, "depth() = %d, max = %d\n", depth(), max);
    for (int l = 0; l <= depth(); l++) {
      Layer * L = tree->L[l];
      fprintf (fp, "Layer level = %d, nc = %d, len = %d\n", l, L->nc, L->len);
      for (int i = 0; i < L->len; i++)
	if (L->m[i]) {
	  fprintf (fp, "  i = %d, refcount = %d\n", i,
		   *((int *)(((char *)L->m[i]) + L->len*sizeof(char *))));
	  for (int j = 0; j < L->len; j++)
	    if (L->m[i][j]) {
	      fprintf (fp, "    j = %d\n", j);
	      fprintf (fp, "point %g %g %d\n",
		       X0 + (i - 1.5)*L0/(1 << l), Y0 + (j - 1.5)*L0/(1 << l),
		       l);
	    }
	}
    }
    fclose (fp);
    fp = fopen ("colls", "w");
    output_cells_internal (fp);
    fclose (fp);
    assert (false);
  }
#endif
}

typedef struct {
  int refined, leaf;
} Remote;

#define REMOTE() ((Remote *)&val(remote,0))

trace
void mpi_boundary_coarsen (int l, int too_fine)
{
  if (npe() == 1)
    return;
  
  check_depth();

  assert (sizeof(Remote) == sizeof(double));
  
  scalar remote[];
  foreach_cell() {
    if (level == l) {
      if (is_local(cell)) {
	REMOTE()->refined = is_refined(cell);
	REMOTE()->leaf = is_leaf(cell);
      }
      else {
	REMOTE()->refined = true;
	REMOTE()->leaf = false;
      }
      continue;
    }
    if (is_leaf(cell))
      continue;
  }
  mpi_boundary_level (mpi_boundary, {remote}, l);
  
  foreach_cell() {
    if (level == l) {
      if (!is_local(cell)) {
	if (is_refined(cell) && !REMOTE()->refined)
	  coarsen_cell_recursive (point, NULL);
	else if (is_leaf(cell) && cell.neighbors && REMOTE()->leaf) {
	  int pid = cell.pid;
	  foreach_child()
	    cell.pid = pid;
	}
      }
      continue;
    }
    if (is_leaf(cell))
      continue;
  }

  check_depth();

  if (l > 0) {
    foreach_cell() {
      if (level == l) {
	remote[] = is_local(cell) ? cell.neighbors : 0;
	continue;
      }
      if (is_leaf(cell))
	continue;
    }
    mpi_boundary_level (mpi_boundary, {remote}, l);
    foreach_cell() {
      if (level == l)
	if (!is_local(cell) && is_local(aparent(0)) && remote[]) {
	  aparent(0).flags &= ~too_fine;
	  continue;
	}
      if (is_leaf(cell))
	continue;
    }
  }
}

static void flag_border_cells()
{
  foreach_cell() {
    if (is_active(cell)) {
      short flags = cell.flags & ~border;
      foreach_neighbor() {
	if (!is_local(cell) || (level > 0 && !is_local(aparent(0)))) {
	  flags |= border; break;
	}
	// root cell
	if (is_refined_check())
	  foreach_child()
	    if (!is_local(cell)) {
	      flags |= border; break;
	    }
	if (flags & border)
	  break;
      }
      cell.flags = flags;
    }
    else {
      cell.flags &= ~border;
      //      continue; // fixme
    }
    if (is_leaf(cell)) {
      if (cell.neighbors) {
	foreach_child()
	  cell.flags &= ~border;
	if (is_border(cell)) {
	  bool remote = false;
	  foreach_neighbor (GHOSTS/2)
	    if (!is_local(cell)) {
	      remote = true; break;
	    }
	  if (remote)
	    foreach_child()
	      cell.flags |= border;
	}
      }
      continue;
    }
  }
}

static int balanced_pid (long index, long nt, int nproc)
{
  long ne = max(1, nt/nproc), nr = nt % nproc;
  int pid = index < nr*(ne + 1) ?
    index/(ne + 1) :
    nr + (index - nr*(ne + 1))/ne;
  return min(nproc - 1, pid);
}

static long restore_mpi_count_cells (FILE * fp, long start, long cell_size)
{
  long index = 0, nt = 0;
  for (int r = 0; r < tree_number_of_roots(); r++) {
    if (fseek (fp, start + index*cell_size + sizeof(unsigned), SEEK_SET) < 0) {
      perror ("restore(): error while seeking root size");
      exit (1);
    }
    double size;
    if (fread (&size, sizeof(double), 1, fp) != 1) {
      fprintf (stderr, "restore(): error: expecting root size\n");
      exit (1);
    }
    long n = size;
    if (n < 1) {
      fprintf (stderr, "restore(): error: invalid root size %g\n", size);
      exit (1);
    }
    nt += n;
    index += n;
  }
  if (fseek (fp, start, SEEK_SET) < 0) {
    perror ("restore(): error while seeking");
    exit (1);
  }
  return nt;
}

// static partitioning: only used for tests
trace
void mpi_partitioning()
{
  prof_start ("mpi_partitioning");

  long nt = 0;
  foreach (serial)
    nt++;

  /* set the pid of each cell */
  long i = 0;
  tree->dirty = true;
  foreach_cell_post (is_active (cell))
    if (is_active (cell)) {
      if (is_leaf (cell)) {
	cell.pid = balanced_pid (i++, nt, npe());
	if (cell.neighbors > 0) {
	  int pid = cell.pid;
	  foreach_child()
	    cell.pid = pid;
	}
	if (!is_local(cell))
	  cell.flags &= ~active;
      }
      else {
	cell.pid = child(0).pid;
	bool inactive = true;
	foreach_child()
	  if (is_active(cell)) {
	    inactive = false; break;
	  }
	if (inactive)
	  cell.flags &= ~active;
      }
    }

  flag_border_cells();
  
  prof_stop();
  
  mpi_boundary_update_buffers();
}

void restore_mpi (FILE * fp, scalar * list1)
{
  long index = 0, start = ftell (fp);
  scalar size[], * list = list_concat ({size}, list1);
  long offset = sizeof(double)*list_len(list);
  long cell_size = sizeof(unsigned) + offset;
  long nt = restore_mpi_count_cells (fp, start, cell_size);

  // read local cells
  static const unsigned short set = 1 << user;
  scalar * listm = is_constant(cm) ? NULL : (scalar *){fm};
  foreach_cell()
    if (balanced_pid (index, nt, npe()) <= pid()) {
      unsigned flags;
      if (fread (&flags, sizeof(unsigned), 1, fp) != 1) {
	fprintf (stderr, "restore(): error: expecting 'flags'\n");
	exit (1);
      }
      for (scalar s in list) {
	double val;
	if (fread (&val, sizeof(double), 1, fp) != 1) {
	  fprintf (stderr, "restore(): error: expecting scalar\n");
	  exit (1);
	}
	if (s.i != INT_MAX)
	  s[] = val;
      }
      cell.pid = balanced_pid (index, nt, npe());
      cell.flags |= set;
      if (!(flags & leaf) && is_leaf(cell)) {
	if (balanced_pid (index + size[] - 1, nt, npe()) < pid()) {
	  fseek (fp, (sizeof(unsigned) + offset)*(size[] - 1), SEEK_CUR);
	  index += size[];
	  continue;
	}
	refine_cell (point, listm, 0, NULL);
      }
      index++;
      if (is_leaf(cell))
	continue;
    }

  // read non-local neighbors
  fseek (fp, start, SEEK_SET);
  index = 0;
  foreach_cell() {
    unsigned flags;
    if (fread (&flags, sizeof(unsigned), 1, fp) != 1) {
      fprintf (stderr, "restore(): error: expecting 'flags'\n");
      exit (1);
    }
    if (cell.flags & set)
      fseek (fp, offset, SEEK_CUR);
    else {
      for (scalar s in list) {
	double val;
	if (fread (&val, sizeof(double), 1, fp) != 1) {
	  fprintf (stderr, "restore(): error: expecting a scalar\n");
	  exit (1);
	}
	if (s.i != INT_MAX)
	  s[] = val;
      }
      cell.pid = balanced_pid (index, nt, npe());
      if (is_leaf(cell) && cell.neighbors) {
	int pid = cell.pid;
	foreach_child()
	  cell.pid = pid;
      }
    }
    if (!(flags & leaf) && is_leaf(cell)) {
      bool locals = false;
      foreach_neighbor(1)
	if ((cell.flags & set) && (is_local(cell) || is_root(point))) {
	  locals = true; break;
	}
      if (locals)
	refine_cell (point, listm, 0, NULL);
      else {
	fseek (fp, (sizeof(unsigned) + offset)*(size[] - 1), SEEK_CUR);
	index += size[];
	continue;
      }
    }
    index++;
    if (is_leaf(cell))
      continue;
  }

  /* set active flags */
  foreach_cell_post (is_active (cell)) {
    cell.flags &= ~set;
    if (is_active (cell)) {
      if (is_leaf (cell)) {
	if (cell.neighbors > 0) {
	  int pid = cell.pid;
	  foreach_child()
	    cell.pid = pid;
	}
	if (!is_local(cell))
	  cell.flags &= ~active;
      }
      else if (!is_local(cell)) {
	bool inactive = true;
	foreach_child()
	  if (is_active(cell)) {
	    inactive = false; break;
	  }
	if (inactive)
	  cell.flags &= ~active;
      }
    }
  }

  flag_border_cells();

  mpi_boundary_update (list);
  free (list);
}

/**
# *z_indexing()*: fills *index* with the Z-ordering index.
   
If `leaves` is `true` only leaves are indexed, otherwise all active
cells are indexed. 

On the master process (`pid() == 0`), the function returns the
(global) maximum index (and -1 on all other processes).

On a single processor, we would just need something
like (for leaves)

~~~literatec
double i = 0;
foreach()
  index[] = i++;
~~~

In parallel, this is a bit more difficult. */

static int root_index(Point point) {
  int ix = point.i - GHOSTS;
#if dimension == 1
  return ix;
#elif dimension == 2
  int iy = point.j - GHOSTS;
  return ix * Dimensions.y + iy;
#else // dimension == 3
  int iy = point.j - GHOSTS;
  int iz = point.k - GHOSTS;
  return (ix * Dimensions.y + iy) * Dimensions.z + iz;
#endif
}

trace
double z_indexing (scalar index, bool leaves)
{
  /**
  We first compute the size of each subtree. */
  
  scalar size[];
  subtree_size (size, leaves);

  /**
  Each level-0 root owner contributes to the global subtree size for a root.
  We use MPI_MAX so duplicated remote copies do not get double-counted within
  a global root. */

  int numroots = tree_number_of_roots();
  double * roots = qcalloc (numroots, double);
  double * offset = qcalloc (numroots, double);

  foreach_cell() {
    if (level == 0 && is_local(cell)) {
      int r = root_index(point);
      if (r >= 0 && r < numroots && size[] > roots[r])
	roots[r] = size[];
      continue;
    }
  }

  mpi_all_reduce_array (roots, MPI_DOUBLE, MPI_MAX, numroots);

  double total = 0.;

  for (int r = 0; r < numroots; r++) {
    offset[r] = total;
    total += roots[r];
  }

  /**
  Seed every allocated real root with its global forest prefix. Child
  propagation below is unchanged. */
  
  foreach_cell() {
    if (level == 0) {
      int r = root_index(point);
      if (r >= 0 && r < numroots)
	index[] = offset[r];
      continue;
    }
  }

  for (int l = 0; l < depth(); l++) {
    boundary_iterate (restriction, {index}, l);
    foreach_cell() {
      if (level == l) {
	if (is_leaf(cell)) {
	  if (is_local(cell) && cell.neighbors) {
	    int i = index[];
	    foreach_child()
	      index[] = i;
	  }
	}
	else { // not leaf
	  bool loc = is_local(cell);
	  if (!loc)
	    foreach_child()
	      if (is_local(cell)) {
		loc = true; break;
	      }
	  if (loc) {
	    int i = index[] + !leaves;
	    foreach_child() {
	      index[] = i;
	      i += size[]; 
	    }
	  }
	}
	continue; // level == l
      }
      if (is_leaf(cell))
	continue;
    }
  }
  boundary_iterate (restriction, {index}, depth());

  free (roots);
  free (offset);

  return pid() == 0 ? total - 1. : -1.;
}
