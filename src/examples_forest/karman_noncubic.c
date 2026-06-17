/**
# Bénard–von Kármán Vortex Street for flow around a cylinder at Re=160

An example of 2D viscous flow around a simple solid boundary. Fluid is
injected to the left of a channel bounded by solid walls with a slip
boundary condition. A passive tracer is injected in the bottom half of
the inlet.

![Animation of the vorticity field.](karman/vort.mp4)(loop)

![Animation of the tracer field.](karman/f.mp4)(loop)

We use the centered Navier-Stokes solver, with embedded boundaries and
advect the passive tracer *f*. */

#include "grid/quadtree.h"

#include "embed.h"
#include "navier-stokes/centered.h"
// #include "navier-stokes/perfs.h"
#include "tracer.h"

scalar f[];
scalar *tracers = {f};
double Reynolds = 160.;
int maxlevel = 6;
face vector muv[];

/**
The domain is eight units long, centered vertically. */

int main() {
  dimensions(8, 1);
  L0 = 8. [1];
  origin(-0.5, -0.5);
  N = 512;
  mu = muv;

  /**
  When using bview we can interactively control the Reynolds number
  and maximum level of refinement. */

  display_control(Reynolds, 10, 1000);
  display_control(maxlevel, 3, 9);

  run();
}

/**
We set a constant viscosity based on the Reynolds number, the cylinder
diameter $D$ and the inflow velocity $U0$. */

double D = 0.125, U0 = 1.;

event properties (i++)
{
  foreach_face()
    muv.x[] = fm.x[]*D*U0/Reynolds;
}

/**
The fluid is injected on the left boundary with velocity $U0$. The
tracer is injected in the lower-half of the left boundary. An outflow
condition is used on the right boundary. */

u.n[left] = dirichlet(U0);
p[left] = neumann(0.);
pf[left] = neumann(0.);
f[left] = dirichlet(y < 0);

u.n[right] = neumann(0.);
p[right] = dirichlet(0.);
pf[right] = dirichlet(0.);

/**
The top and bottom walls are free-slip and the cylinder is no-slip. */

u.n[top] = dirichlet(0.);
u.t[top] = neumann(0.);
uf.n[top] = 0.;
u.n[bottom] = dirichlet(0.);
u.t[bottom] = neumann(0.);
uf.n[bottom] = 0.;

u.n[embed] = dirichlet(0.);
u.t[embed] = dirichlet(0.);

event init(t = 0) {
  /**
  The domain is a circle of diameter 0.125. */
  solid(cs, fs, sqrt(sq(x) + sq(y)) - D / 2.);

  /**
  We set the initial velocity field. */

  foreach ()
    u.x[] = cs[] ? U0 : 0.;
}

/**
We check the number of iterations of the Poisson and viscous
problems. */

event logfile (i++)
  fprintf (stderr, "%d %g %d %d\n", i, t, mgp.i, mgu.i);

/**
We produce animations of the vorticity and tracer fields... */

// event movies(i += 4; t <= 15.) {
  // scalar omega[];
  // vorticity(u, omega);

  // char fname[128];
  // snprintf(fname, 128, "karman_%d.vtkhdf", i);
  // vtkHDFHyperTreeGrid vtk_hdf = vtk_HDF_hypertreegrid_init(all,NULL,fname);
  // vtk_HDF_hypertreegrid_close(&vtk_hdf);
// }

/**
We adapt according to the error on the embedded geometry, velocity and
tracer fields. */

#if TREE
event adapt(i++) {
  adapt_wavelet({cs, u, f}, (double[]){1e-2, 3e-2, 3e-2, 3e-2}, maxlevel, 1);
}
#endif

event end(i++; t <= 15.) {}

/**
## See also

* [Same example with
Gerris](https://gerris.dalembert.upmc.fr/gerris/examples/examples/cylinder.html)
*/
