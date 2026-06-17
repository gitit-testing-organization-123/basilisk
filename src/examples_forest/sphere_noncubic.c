// src/examples_forest/sphere_noncubic.c
#include "grid/octree.h"
#include "embed.h"
#include "navier-stokes/centered.h"

int maxlevel = 5;
face vector muv[];

int main()
{
  dimensions (4, 2, 1);   // noncubic 3D forest
  size (4.);
  origin (-1., -1., -0.5);
  init_grid (16);
  mu = muv;
  run();
}

event properties (i++)
  foreach_face()
    muv.x[] = fm.x[]/300.;

u.n[left] = dirichlet(1.);
p[left] = neumann(0.);
pf[left] = neumann(0.);

u.n[right] = neumann(0.);
p[right] = dirichlet(0.);
pf[right] = dirichlet(0.);

u.n[embed] = dirichlet(0.);
u.t[embed] = dirichlet(0.);
u.r[embed] = dirichlet(0.);

event init (i = 0)
{
  refine (sq(x - 0.5) + sq(y) + sq(z) < sq(0.7) && level < maxlevel);
  solid (cs, fs, sq(x - 0.5) + sq(y) + sq(z) - sq(0.25));
  foreach()
    u.x[] = cs[] ? 1. : 0.;
}

event logfile (i++)
  fprintf (stderr, "%d %g %d %d\n", i, t, mgp.i, mgu.i);

event adapt (i++)
  adapt_wavelet ({cs,u}, (double[]){1e-2, 3e-2, 3e-2, 3e-2},
                 maxlevel, 1);

event end (i++; t <= 60) {}
