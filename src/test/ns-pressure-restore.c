#include "grid/multigrid.h"
#include "navier-stokes/centered.h"

static double pexact (double x, double y)
{
  double X = x/L0, Y = y/L0;
  return 3. + 2.*X - Y + 0.5*X*Y;
}

static double pfexact (double x, double y)
{
  double X = x/L0, Y = y/L0;
  return -1. + X*X + 0.25*Y;
}

static int restored = 0;

int main()
{
  origin (-0.5, -0.5);
  init_grid (16);
  DT = 0.1;
  run();
}

event init (i = 0)
{
  p.nodump = pf.nodump = false;
  restored = restore (file = "ns-pressure.dump", list = {p, pf});
  if (restored) {
    foreach() {
      assert (fabs (p[] - pexact (x, y)) < 1e-12);
      assert (fabs (pf[] - pfexact (x, y)) < 1e-12);
    }
    fprintf (stderr, "restored: i=%d t=%g dt=%g\n", i, t, dt);
  }
  else {
    foreach() {
      u.x[] = sin(2.*pi*x/L0)*cos(2.*pi*y/L0);
      u.y[] = - cos(2.*pi*x/L0)*sin(2.*pi*y/L0);
      p[] = 0.;
      pf[] = 0.;
    }
  }
}

event stamp_pressure (i = 3)
{
  p.nodump = pf.nodump = false;
  foreach() {
    p[] = pexact (x, y);
    pf[] = pfexact (x, y);
  }
  dump (file = "ns-pressure.dump", list = {p, pf});
  fprintf (stderr, "dumped: i=%d t=%g dt=%g\n", i, t, dt);
}

event trace_dt (i <= 6; i++)
  fprintf (stderr, "trace: restored=%d i=%d t=%g dt=%g\n", restored, i, t, dt);

event stop (i = 6)
  return 1;
