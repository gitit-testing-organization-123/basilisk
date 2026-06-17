#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
BASILISK_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd -P)"

if [ "$#" -eq 0 ]; then
  printf 'usage: %s source.c [source2.c ...]\n' "$(basename "$0")" >&2
  printf 'env: NP=2 RUN_ARGS="..." MPIRUN=... QCC_FLAGS="..."\n' >&2
  exit 2
fi

QCC="${BASILISK_ROOT}/build/release/src/qcc-toolchain"
MPIRUN="${MPIRUN:-mpirun}"
CC="${CC:-mpicc -std=c99}"
NP="${NP:-4}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/.build/debug}"
RUN_ARGS="${RUN_ARGS:-}"
QCC_FLAGS="${QCC_FLAGS:-}"

if [ ! -x "${QCC}" ]; then
  printf 'error: qcc-toolchain not found or not executable: %s\n' "${QCC}" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}"

for src in "$@"; do
  if [ ! -f "${src}" ] && [ -f "${SCRIPT_DIR}/${src}" ]; then
    src="${SCRIPT_DIR}/${src}"
  fi
  if [ ! -f "${src}" ]; then
    printf 'error: source not found: %s\n' "${src}" >&2
    exit 1
  fi

  name="$(basename "${src%.*}")"
  out="${BUILD_DIR}/${name}"

  printf '==> compiling %s\n' "${src}"
  CC="${CC}" \
  "${QCC}" \
    -D_MPI=1 \
    ${QCC_FLAGS} \
    "${src}" \
    -o "${out}" \
    -I"${SCRIPT_DIR}" \
    -I"${BASILISK_ROOT}/src" \
    -lm \
    -lmpi

  printf '==> running %s with %s rank(s)\n' "${out}" "${NP}"
  # shellcheck disable=SC2086
  "${MPIRUN}" -np "${NP}" "${out}" ${RUN_ARGS}
done
