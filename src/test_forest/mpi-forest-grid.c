#include "grid/quadtree.h"

static double xperiod, yperiod;

static double field_value(double xp, double yp) {
  return sin(2.*pi*(xp - X0)/xperiod)*cos(2.*pi*(yp - Y0)/yperiod);
}

int main() {
  dimensions(3, 2);
  foreach_dimension()
    periodic(right);

  L0 = 6.*pi;
  origin(-3.*pi, -2.*pi);
  xperiod = L0;
  yperiod = L0*((double)Dimensions.y/Dimensions.x);

  init_grid(12);

  vector u[];
  scalar s[];
  foreach()
    u.x[] = u.y[] = s[] = field_value(x, y);

  boundary({s, u});

  foreach()
    foreach_neighbor(2) {
      double v = field_value(x, y);
      assert(fabs(s[] - v) < 1e-12);
      assert(fabs(u.x[] - v) < 1e-12);
      assert(fabs(u.y[] - v) < 1e-12);
    }

  if (pid() == 0)
    fprintf(stderr, "mpi-forest-grid: ok\n");
}
