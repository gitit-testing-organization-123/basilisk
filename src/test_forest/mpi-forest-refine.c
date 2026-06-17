#include "grid/quadtree.h"
#include "test/refine_unbalanced.h"

int main() {
  dimensions(3, 2);
  foreach_dimension()
    periodic(right);

  L0 = 6.*pi;
  origin(-3.*pi, -2.*pi);

  init_grid(12);

  scalar s[];
  foreach()
    s[] = 1.;
  boundary({s});

  refine_unbalanced(level < 4 &&
                    (sq(x + pi) + sq(y + pi/3.) < sq(2.4) ||
                     (x > 0. && y > -pi && y < pi/2.)), {s});

  foreach()
    s[] = 1.;
  boundary({s});

  foreach()
    foreach_neighbor(2)
      if (allocated(0))
        assert(s[] == 1.);

  scalar index[];
  z_indexing(index, true);
  foreach()
    assert(index[] >= 0.);

  if (pid() == 0)
    fprintf(stderr, "mpi-forest-refine: ok\n");
}
