# `tree-mpi-old.h` Exceptions in Noncubic Forest MPI Domains

This note records the MPI buffer cases where the old sender-side logic in
`src/grid/tree-mpi-old.h` is not reliable for noncubic forest domains.

## Context

The old implementation builds `snd` lists by guessing, from local cells, which
remote ranks will need data.  This works reasonably for the original single-root
tree topology, but forests add multiple real level-0 roots.  A required ghost can
then be a same-level root neighbor, with no shared parent cell above it.

The newer request-driven code avoids this by making the receiver authoritative:
the rank that needs a point puts it in `rcv`, then the owner mirrors that request
into `snd`.

## Observed Exceptions

### Adjacent Level-0 Root Needed as a Coarse Stencil

Forest `karman.c` with `dimensions(8, 1)` failed in embedded prolongation:

```text
src/embed-tree.h:280:
Assertion `coarse(cs,child.x) && coarse(cs,0,child.y)' failed.
```

The failing cell was a level-1 child.  The missing value was not a fine child or
sibling; it was a neighboring level-0 root owned by another rank.  The old
`root_pids()`/`locals_pids()` logic does not naturally classify an adjacent real
root as something to send for a level-1 coarse stencil.

### Remote Level-0 Root with Local Children

The stock single-root `karman.c` exposed the related root case:

```text
src/embed-tree.h:274:
Assertion `coarse(cs)' failed.
```

Ranks had local level-1 children under a root owned by rank 0, but the root
geometry (`cs`, `fs`, etc.) was not available on the non-owner ranks during the
first `adapt_wavelet()` prolongation.  In forests this same pattern can occur for
any remote level-0 root that has local descendants.

### Restriction Root Exchange Across Real Roots

`mpi-forest-restriction.c` exercises the restriction path after refinement in a
noncubic root layout.  The important exception is that a receiver can need a
level-0 root value from a rank that the sender-side traversal would not infer
from local children alone.  This is especially easy to miss after coarsening or
root ownership changes.

## Why These Are Special

Single-root trees have one real root, so there are no same-level root neighbors.
All root-related MPI data is effectively under one parentless top cell.  Forests
have many parentless top cells, so level-0 adjacency is real topology, not just a
boundary condition.

The old logic mostly reasons through:

- local children,
- remote children of a parent,
- remote siblings/children at `level > 0`.

It does not fully cover:

- a remote level-0 root used directly as `coarse(...)`,
- an adjacent remote level-0 root used by a level-1 child stencil,
- level-0 root data needed by both the restriction and level/root exchanges.

## Current Fix Direction

For level-0 roots, request the needed root points from the receiver side:

- request the remote root itself when it has local descendants;
- request adjacent remote roots when they can be used by level-1 coarse stencils;
- place those requests in both `restriction->rcv` and `mpi_level_root->rcv`.

For all levels, mirroring receive requests into send lists avoids sender/receiver
matrix mismatches caused by sender-side guessing.
