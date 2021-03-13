FROM testillano/nghttp2_build:latest
MAINTAINER testillano

LABEL testillano.http2comm_build.description="Docker image to build ert_http2comm library"

WORKDIR /code/build

ARG make_procs=4
ARG build_type=Release
ARG ert_logger_ver=v1.0.7

# ert_logger
RUN set -x && \
    wget https://github.com/testillano/logger/archive/${ert_logger_ver}.tar.gz && tar xvf ${ert_logger_ver}.tar.gz && cd logger-*/ && \
    cmake -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

