#include "grid/quadtree.h"

#define BGHOSTS 2

scalar s[];

int main(int argc, char *argv[]) {
  dimensions(4, 1);
  origin(-0.5, -0.5);
  L0 = 4.;
  init_grid(argc > 1 ? atoi(argv[1]) : 32);

  unrefine(sq(x - 0.1) + sq(y - 0.1) > sq(0.12));

  foreach()
    s[] = 1.;
  boundary({s});

  foreach()
    foreach_neighbor(1)
      if (allocated(0))
        assert(s[] == 1.);

  scalar index[];
  z_indexing(index, true);
  foreach()
    assert(index[] >= 0.);

  if (pid() == 0)
    fprintf(stderr, "mpi-forest-coarsen: ok\n");
}
