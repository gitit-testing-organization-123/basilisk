#include "grid/quadtree.h"
#include "utils.h"

static double field_value (double xp, double yp)
{
  return 1. + 0.25*xp - 0.5*yp + sin (xp + 0.5*yp);
}

static bool refine_region (double xp, double yp)
{
  return (sq(xp + 2.1) + sq(yp + 0.5) < sq(1.2) ||
	  sq(xp - 0.4) + sq(yp - 0.6) < sq(0.9) ||
	  sq(xp - 2.0) + sq(yp + 0.2) < sq(0.7));
}

int main()
{
  dimensions (3, 2);
  L0 = 6.;
  origin (-3., -2.);
  init_grid (12);

  scalar s[];

  refine (level < 4 && refine_region (x, y));
  foreach()
    s[] = field_value (x, y);
  boundary ({s});

  long leaves_before = 0;
  double sum_before = 0.;
  foreach (reduction(+:leaves_before) reduction(+:sum_before)) {
    leaves_before++;
    sum_before += s[];
  }

  dump (file = "forest-dump-restore.dump", list = {s}, unbuffered = true);

  dimensions (1, 1);
  init_grid (1);

  assert (restore (file = "forest-dump-restore.dump", list = {s}));

  assert (Dimensions.x == 3);
  assert (Dimensions.y == 2);

  long leaves_after = 0;
  double sum_after = 0.;
  foreach (reduction(+:leaves_after) reduction(+:sum_after)) {
    leaves_after++;
    sum_after += s[];
    assert (fabs (s[] - field_value (x, y)) < 1e-12);
  }

  assert (leaves_after == leaves_before);
  assert (fabs (sum_after - sum_before) < 1e-12);

  if (pid() == 0)
    fprintf (stderr, "mpi-forest-dump-restore: ok\n");
}
