#!/bin/bash

#############
# VARIABLES #
#############
base_ver__dflt=latest # nghttp2_build
make_procs__dflt=$(grep processor /proc/cpuinfo -c)
build_type__dflt=Release
ert_logger_ver__dflt=v1.0.7

#############
# EXECUTION #
#############
cd $(dirname $0)
echo
echo "---------------------"
echo "Build 'builder image'"
echo "---------------------"
echo
echo "Input 'make_procs' [${make_procs__dflt}]:"
read make_procs
[ -z "${make_procs}" ] && make_procs=${make_procs__dflt}
bargs="--build-arg make_procs=${make_procs}"

echo "Input nghttp2_build 'base_ver' [${base_ver__dflt}]:"
read base_ver
[ -z "${base_ver}" ] && base_ver=${base_ver__dflt}
bargs+=" --build-arg base_ver=${base_ver}"

echo "Input ert_logger version [${ert_logger_ver__dflt}]:"
read ert_logger_ver
[ -z "${ert_logger_ver}" ] && ert_logger_ver=${ert_logger_ver__dflt}
bargs+=" --build-arg ert_logger_ver=${ert_logger_ver__dflt}"

docker build --rm ${bargs} -t testillano/http2comm_build .

echo
echo "-------------"
echo "Build project"
echo "-------------"
echo
echo "Input build type (Release|Debug) [${build_type__dflt}]:"
read build_type
[ -z "${build_type}" ] && build_type=${build_type__dflt}

rm -f CMakeCache.txt
envs="-e MAKE_PROCS=${make_procs} -e BUILD_TYPE=${build_type}"
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/http2comm_build
docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code testillano/http2comm_build "" doc

