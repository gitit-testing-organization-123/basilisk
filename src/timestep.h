double dt_previous = 0.;

// note: u is weighted by fm
double timestep (const face vector u, double dtmax)
{ 
  if (t == 0.) dt_previous = 0.;
  dtmax /= CFL;
  foreach_face(reduction(min:dtmax))
    if (u.x[] != 0.) {
      double dt = Delta/fabs(u.x[]);
      assert (fm.x[]);
      dt *= fm.x[];
      if (dt < dtmax) dtmax = dt;
    }
  dtmax *= CFL;
  if (dtmax > dt_previous)
    dtmax = (dt_previous + 0.1*dtmax)/1.1;
  dt_previous = dtmax;
  return dtmax;
}
