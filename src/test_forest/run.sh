#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

: "${QCC:=qcc}"
: "${CC:=cc}"
: "${CFLAGS:=}"
: "${LDFLAGS:=-lm}"

tests=(
  forest_traversal
  forest_locate
  forest_refine
  forest_periodic
  forest_cache_2d
  forest_solid_geometry
  forest_embed_projection
)

mpi_tests=(
  forest_z_indexing
)

mkdir -p .build

for test in "${tests[@]}"; do
  src="${test}.c"
  exe=".build/${test}"

  printf '==> compiling %s\n' "$src"
  "$QCC" ${CFLAGS} "$src" -o "$exe" ${LDFLAGS}

  printf '==> running %s\n' "$test"
  "$exe"
done

for test in "${mpi_tests[@]}"; do
  src="${test}.c"
  exe=".build/${test}"

  printf '==> compiling %s with _MPI=1\n' "$src"
  "$QCC" ${CFLAGS} -D_MPI=1 "$src" -o "$exe" ${LDFLAGS}

  printf '==> running %s\n' "$test"
  "$exe"
done

printf 'all forest tests passed\n'
