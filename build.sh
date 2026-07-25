#!/bin/bash
# =============================================================================
# http2comm build script
# =============================================================================
# Builds the http2comm Docker image. Non-interactive.
# All versions are read from the Dockerfile (single source of truth).
#
# Usage:
#   ./build.sh                              # build with defaults (all targets)
#   ./build.sh --builder                    # deps stage only
#   ert_metrics_ver=v1.3.0 ./build.sh      # override a version
#   DBUILD_XTRA_OPTS=--no-cache ./build.sh  # force rebuild
#
# Environment variables (override any version):
#   image_tag, make_procs, build_type, boost_ver, nghttp2_ver, nghttp2_asio_ver,
#   ert_nghttp2_ver, ert_logger_ver, ert_queuedispatcher_ver,
#   jupp0r_prometheuscpp_ver, civetweb_civetweb_ver, ert_metrics_ver
#
# Other variables:
#   DBUILD_XTRA_OPTS: extra docker build options
# =============================================================================

set -e

SCR="$(readlink -f "$0")"
cd "$(dirname "${SCR}")"

DOCKERFILE=Dockerfile
registry=ghcr.io/testillano

# Parse version from Dockerfile, allow env override
parse_arg() { grep "^ARG ${1}=" "${DOCKERFILE}" | head -1 | cut -d= -f2; }
resolve() {
  local val="${!1}"
  [ -z "${val}" ] && val="$(parse_arg "$1")"
  [ -z "${val}" ] && val="$(eval echo \$${1}__dflt)"
  echo "${val}"
}

# Defaults for non-Dockerfile vars
image_tag__dflt=latest
make_procs__dflt=$(nproc)

build_bargs() {
  local bargs=""
  bargs+=" --build-arg make_procs=$(resolve make_procs)"
  bargs+=" --build-arg build_type=$(resolve build_type)"
  bargs+=" --build-arg boost_ver=$(resolve boost_ver)"
  bargs+=" --build-arg nghttp2_ver=$(resolve nghttp2_ver)"
  bargs+=" --build-arg nghttp2_asio_ver=$(resolve nghttp2_asio_ver)"
  bargs+=" --build-arg ert_nghttp2_ver=$(resolve ert_nghttp2_ver)"
  bargs+=" --build-arg ert_logger_ver=$(resolve ert_logger_ver)"
  bargs+=" --build-arg ert_queuedispatcher_ver=$(resolve ert_queuedispatcher_ver)"
  bargs+=" --build-arg jupp0r_prometheuscpp_ver=$(resolve jupp0r_prometheuscpp_ver)"
  bargs+=" --build-arg civetweb_civetweb_ver=$(resolve civetweb_civetweb_ver)"
  bargs+=" --build-arg ert_metrics_ver=$(resolve ert_metrics_ver)"
  echo "${bargs}"
}

build_builder() {
  echo
  echo "=== Build http2comm_builder (deps stage) ==="
  echo
  local tag=$(resolve image_tag)
  local bargs=$(build_bargs)

  set -x
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} \
    --target deps \
    -t ${registry}/http2comm_builder:"${tag}" . || return 1
  set +x
}

build_image() {
  echo
  echo "=== Build http2comm image (deps + library) ==="
  echo
  local tag=$(resolve image_tag)
  local bargs=$(build_bargs)

  set -x
  # shellcheck disable=SC2086
  docker build --rm ${DBUILD_XTRA_OPTS} ${bargs} \
    --target build \
    -t ${registry}/http2comm:"${tag}" . || return 1
  set +x

  # Also tag the builder from cache
  echo
  echo "Tagging builder image from cache..."
  # shellcheck disable=SC2086
  docker build --rm ${bargs} \
    --target deps \
    -t ${registry}/http2comm_builder:"${tag}" . 2>/dev/null || true
}

build_all() {
  build_image
}

#############
# EXECUTION #
#############
case "${1:-}" in
  --builder) build_builder ;;
  --image) build_image ;;
  -h|--help)
    echo "Usage: $0 [--builder|--image]"
    echo ""
    echo "  (no args): builds everything (deps + library)."
    echo "  --builder: builds only deps stage."
    echo "  --image:   builds deps + library (same as no args)."
    echo ""
    echo "Examples:"
    echo "  $0"
    echo "  ert_metrics_ver=v1.3.0 $0"
    echo "  DBUILD_XTRA_OPTS=--no-cache $0"
    ;;
  "") build_all ;;
  *) echo "Unknown option: $1" && exit 1 ;;
esac

exit $?
