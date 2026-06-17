#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

: "${NP:=4}"
export NP

./compile-mpi-debug.sh \
  mpi-forest-grid.c \
  mpi-forest-refine.c \
  mpi-forest-coarsen.c \
  mpi-forest-restriction.c
