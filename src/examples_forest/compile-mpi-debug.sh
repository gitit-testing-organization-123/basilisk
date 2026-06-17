#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
BASILISK_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd -P)"

if [ "$#" -eq 0 ]; then
  printf 'usage: %s source.c [source2.c ...]\n' "$(basename "$0")" >&2
  printf 'env: NP=2 RUN_ARGS="..." MPIRUN=... QCC_FLAGS="..."\n' >&2
  printf '     TRACE_LEVEL=2|3 TRACE_OUTPUT_DIR=/tmp/trace\n' >&2
  printf '     MTRACE_LEVEL=1|2|3 MTRACE_PREFIX=/tmp/name MTRACE_STATS_DIR=/tmp/stats\n' >&2
  exit 2
fi

QCC="${BASILISK_ROOT}/build/release/src/qcc-toolchain"
MPIRUN="${MPIRUN:-mpirun}"
CC="${CC:-mpicc -std=c99 -g -O0}"
NP="${NP:-4}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/.build/debug}"
RUN_ARGS="${RUN_ARGS:-}"
QCC_FLAGS="${QCC_FLAGS:-}"
TRACE_LEVEL="${TRACE_LEVEL:-0}"
TRACE_OUTPUT_DIR="${TRACE_OUTPUT_DIR:-${BUILD_DIR}/trace}"
MTRACE_LEVEL="${MTRACE_LEVEL:-0}"
MTRACE_PREFIX="${MTRACE_PREFIX:-}"
MTRACE_STATS_DIR="${MTRACE_STATS_DIR:-${BUILD_DIR}/mtrace-stats}"

if [ "${TRACE_LEVEL}" != "0" ]; then
  QCC_FLAGS="${QCC_FLAGS} -DTRACE=${TRACE_LEVEL}"
fi

if [ "${MTRACE_LEVEL}" != "0" ]; then
  QCC_FLAGS="${QCC_FLAGS} -DMTRACE=${MTRACE_LEVEL}"
fi

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
    -g \
    -O0 \
    ${QCC_FLAGS} \
    "${src}" \
    -o "${out}" \
    -I"${SCRIPT_DIR}" \
    -I"${BASILISK_ROOT}/src" \
    -lm \
    -lmpi

  printf '==> running %s with %s rank(s)\n' "${out}" "${NP}"
  if [ "${TRACE_LEVEL}" != "0" ] || [ "${MTRACE_LEVEL}" != "0" ]; then
    env_args=()
    if [ "${TRACE_LEVEL}" != "0" ]; then
      mkdir -p "${TRACE_OUTPUT_DIR}"
      trace_base="${TRACE_OUTPUT_DIR}/${name}.trace"
      printf '==> writing trace stdout profiling to %s.<rank>\n' "${trace_base}"
      env_args+=("TRACE_OUTPUT_BASE=${trace_base}")
    fi
    if [ "${MTRACE_LEVEL}" != "0" ]; then
      mkdir -p "${MTRACE_STATS_DIR}"
      stats_base="${MTRACE_STATS_DIR}/${name}.stats"
      trace_prefix="${MTRACE_PREFIX:-${MTRACE_STATS_DIR}/${name}.trace}"
      printf '==> writing mtrace stderr summaries to %s.<rank>\n' "${stats_base}"
      env_args+=("MTRACE_STATS_BASE=${stats_base}")
      if [ "${MTRACE_LEVEL}" = "1" ]; then
        printf '==> writing mtrace allocation traces to %s[-rank]\n' "${trace_prefix}"
        env_args+=("MTRACE=${trace_prefix}")
      fi
    fi
    # shellcheck disable=SC2086
    "${MPIRUN}" -np "${NP}" env "${env_args[@]}" \
      sh -c 'rank=${OMPI_COMM_WORLD_RANK:-${PMI_RANK:-${PMIX_RANK:-0}}}; if [ -n "${TRACE_OUTPUT_BASE:-}" ] && [ -n "${MTRACE_STATS_BASE:-}" ]; then exec "$@" > "${TRACE_OUTPUT_BASE}.${rank}" 2> "${MTRACE_STATS_BASE}.${rank}"; elif [ -n "${TRACE_OUTPUT_BASE:-}" ]; then exec "$@" > "${TRACE_OUTPUT_BASE}.${rank}"; elif [ -n "${MTRACE_STATS_BASE:-}" ]; then exec "$@" 2> "${MTRACE_STATS_BASE}.${rank}"; else exec "$@"; fi' \
      sh "${out}" ${RUN_ARGS}
  else
    # shellcheck disable=SC2086
    "${MPIRUN}" -np "${NP}" "${out}" ${RUN_ARGS}
  fi
done
