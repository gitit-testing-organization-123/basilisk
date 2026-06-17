#include "grid/quadtree.h"

static double field_value(double xp, double yp) {
  return 2.*xp - 3.*yp + 0.5;
}

int main() {
  dimensions(3, 2);
  foreach_dimension()
    periodic(right);

  L0 = 6.;
  origin(-3., -2.);

  init_grid(12);

  scalar s[];
  refine(level < 4 &&
         (sq(x + 1.2) + sq(y + 0.4) < sq(1.4) ||
          sq(x - 1.1) + sq(y - 0.2) < sq(1.2)));

  foreach()
    s[] = field_value(x, y);
  boundary({s});
  restriction({s});

  foreach_cell() {
    if (cell.pid >= 0 && is_local(cell))
      assert(fabs(s[] - field_value(x, y)) < 1e-12);
    if (is_leaf(cell))
      continue;
  }

  if (pid() == 0)
    fprintf(stderr, "mpi-forest-restriction: ok\n");
}
