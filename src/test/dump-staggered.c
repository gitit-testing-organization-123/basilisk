/**
# Dump/restore of staggered fields

Checks that centered, face and vertex fields are restored with their
full state. */

#include "utils.h"

static double cexact (double x, double y)
{
  return 1. + 2.*x - 3.*y + 0.25*x*y;
}

static double uxexact (double x, double y)
{
  return 10. + x + 2.*y;
}

static double uyexact (double x, double y)
{
  return -7. + 3.*x - y;
}

static double phiexact (double x, double y)
{
  return 4. + x*x - 2.*y;
}

int main()
{
  size (2.);
  origin (-0.5, -0.25);
  init_grid (8);
#if TREE
  refine (level == 3 && x < 0.25 && y < 0.25);
#endif

  scalar c[];
  face vector u[];
  vertex scalar phi[];

  foreach()
    c[] = cexact (x, y);
  foreach_face (x)
    u.x[] = uxexact (x, y);
  foreach_face (y)
    u.y[] = uyexact (x, y);
  foreach_vertex()
    phi[] = phiexact (x, y);

  dump (file = "dump-staggered.dump", list = {c, u, phi});

  foreach()
    c[] = -1.;
  foreach_face()
    u.x[] = -1.;
  foreach_vertex()
    phi[] = -1.;

  assert (restore (file = "dump-staggered.dump", list = {c, u, phi}));

  foreach()
    assert (fabs (c[] - cexact (x, y)) < 1e-12);
  foreach_face (x)
    assert (fabs (u.x[] - uxexact (x, y)) < 1e-12);
  foreach_face (y)
    assert (fabs (u.y[] - uyexact (x, y)) < 1e-12);
  foreach_vertex()
    assert (fabs (phi[] - phiexact (x, y)) < 1e-12);
}
