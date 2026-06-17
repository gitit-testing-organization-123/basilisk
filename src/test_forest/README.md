# Forest tree tests

These tests exercise serial non-cubic tree behavior

Run them with `qcc` on `PATH`:

```sh
cd src/test_forest
./run.sh
```

The intended coverage is:

- `forest_traversal.c`: root count/order, leaf count, total cell count,
  `foreach_cell_all()` root apron.
- `forest_locate.c`: rectangular physical coordinates and `locate()` mapping.
- `forest_refine.c`: selective refinement of one root in a multi-root forest.
- `forest_periodic.c`: periodic-axis trimming of the root apron.
