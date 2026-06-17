#ifndef TEST_FOREST_REFINE_UNBALANCED_H
#define TEST_FOREST_REFINE_UNBALANCED_H

#define refine_unbalanced(cond, list) do {				\
    tree->refined.n = 0;						\
    int refined;							\
    do {								\
      refined = 0;							\
      foreach_leaf()							\
	if (cond) {							\
	  refine_cell(point, list, 0, &tree->refined);			\
	  refined++;							\
	}								\
    } while (refined);							\
    mpi_boundary_refine(list);						\
    mpi_boundary_update_buffers();					\
    grid->tn = 0;							\
    boundary(list);							\
  } while (0)

#endif
