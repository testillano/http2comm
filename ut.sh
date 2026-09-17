#!/bin/bash
# =============================================================================
# http2comm unit-test runner
# =============================================================================
# Compiles and runs the library unit tests inside the builder image (which has
# all dependencies installed), under AddressSanitizer. Nothing is installed on
# the host and no image is published.
#
# These tests focus on teardown safety (Http2Connection), which is hard to
# cover at integration level and is where subtle concurrency/lifetime bugs live.
#
# Usage:
#   ./ut.sh                       # build builder if needed, compile + run UTs (ASAN)
#   ./ut.sh --gtest_filter=...    # extra args are forwarded to the test binary
#   base_tag=latest ./ut.sh       # override builder image tag
#
# Requirements: docker, and the builder image (ghcr.io/testillano/http2comm_builder).
# If the builder image is missing it is built via ./build.sh --builder.
# =============================================================================

set -e

SCR="$(readlink -f "$0")"
ROOT="$(dirname "${SCR}")"
cd "${ROOT}"

registry=ghcr.io/testillano
base_tag="${base_tag:-latest}"
builder="${registry}/http2comm_builder:${base_tag}"

# Ensure builder image (with deps) exists.
if ! docker image inspect "${builder}" >/dev/null 2>&1; then
  echo "Builder image '${builder}' not found; building it (./build.sh --builder) ..."
  ./build.sh --builder
fi

echo
echo "=== Compiling and running http2comm unit tests (ASAN) ==="
echo

# Run inside an ephemeral container: mount the source read-write (only /tmp
# artifacts are produced), compile the UT against the installed deps, run it.
# --cap-add SYS_PTRACE / seccomp unconfined so ASAN works reliably in Docker.
docker run --rm \
  --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
  -v "${ROOT}":/http2comm \
  -w /http2comm/ut \
  --entrypoint bash \
  "${builder}" -c '
    set -e
    # gtest may or may not be present in the builder; install if missing.
    if ! ls /usr/local/lib/libgtest.a /usr/lib/x86_64-linux-gnu/libgtest.a >/dev/null 2>&1; then
      apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq libgtest-dev >/dev/null 2>&1 || true
    fi

    SRCS="$(ls *_test.cpp)"
    echo "Test sources: ${SRCS}"

    g++ -std=c++17 -g -O1 -fsanitize=address -fno-omit-frame-pointer \
      -I/http2comm/include -I/usr/local/include \
      ${SRCS} /http2comm/src/Http2Connection.cpp \
      -L/usr/local/lib/ert \
      -lgtest -lgtest_main -lnghttp2_asio -lnghttp2 -lert_logger \
      -lboost_system -lssl -lcrypto -lpthread \
      -o /tmp/http2comm_ut

    export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH}
    # detect_leaks=0: we assert teardown safety (no UAF/races), not leak-freedom.
    ASAN_OPTIONS=detect_leaks=0 /tmp/http2comm_ut '"$*"'
  '
