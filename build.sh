#!/bin/bash

#############
# VARIABLES #
#############
image_tag__dflt=latest
base_tag__dflt=latest
make_procs__dflt=$(grep processor /proc/cpuinfo -c)
build_type__dflt=Release
ert_logger_ver__dflt=v1.0.7

#############
# FUNCTIONS #
#############
usage() {
  cat << EOF

  Usage: $0 [--project-image|--builder-image|--library]

         --project-image: builds project image from './Dockerfile'.
         --builder-image: builds base image from './Dockerfile.build'.
         --library:       builds the project library using builder image.

         For headless mode, prepend/export asked variables:

         --project-image: image_tag, base_tag (http2comm_builder), make_procs, build_type
         --builder-image: image_tag, base_tag (nghttp2), make_procs, build_type, ert_logger_ver

         or environment variables towards docker run:

         --library:       make_procs, build_type

EOF
}

# $1: variable by reference; $2: default value
_read() {
  local -n varname=$1
  local default=$2

  local s_default="<null>"
  [ -n "${default}" ] && s_default="${default}"
  echo "Input '$1' value [${s_default}]:"

  if [ -n "${varname}" ]
  then
    echo "${varname}"
  else
    read varname
    [ -z "${varname}" ] && varname=${default}
  fi
}

build_project_image() {
  echo
  echo "=== Build http2comm image ==="
  echo
  _read image_tag ${image_tag__dflt}
  _read base_tag ${base_tag__dflt}
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"

  set -x
  docker build --rm ${bargs} -t testillano/http2comm:${image_tag} . || return 1
  set +x
}

build_builder_image() {
  echo
  echo "=== Build http2comm_builder image ==="
  echo
  _read image_tag ${image_tag__dflt}
  _read base_tag ${base_tag__dflt}
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}
  _read ert_logger_ver ${ert_logger_ver__dflt}

  bargs="--build-arg base_tag=${base_tag}"
  bargs+=" --build-arg make_procs=${make_procs}"
  bargs+=" --build-arg build_type=${build_type}"
  bargs+=" --build-arg ert_logger_ver=${ert_logger_ver}"

  set -x
  docker build --rm ${bargs} -f Dockerfile.build -t testillano/http2comm_builder:${image_tag} . || return 1
  set +x
}

build_library() {
  echo
  echo "=== Build http2comm library ==="
  echo
  _read make_procs ${make_procs__dflt}
  _read build_type ${build_type__dflt}

  rm -f CMakeCache.txt
  envs="-e MAKE_PROCS=${make_procs} -e BUILD_TYPE=${build_type}"

  set -x
  docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/http2comm_builder || return 1
  docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/http2comm_builder "" doc || return 1
  set +x
}
#############
# EXECUTION #
#############
cd $(dirname $0)

case "$1" in
  --project-image) build_project_image ;;
  --builder-image) build_builder_image ;;
  --library) build_library ;;
  *) usage && exit 1 ;;
esac

exit $?

