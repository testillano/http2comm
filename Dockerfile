# =============================================================================
# http2comm multi-stage Dockerfile
# =============================================================================
# Single file replacing the inheritance from nghttp2 image.
# All dependency versions are declared as ARGs here (single source of truth).
#
# Stages:
#   deps  - All third-party libraries compiled and installed
#   build - http2comm library compilation and installation
#
# Usage:
#   docker build --target deps  -t http2comm_builder .
#   docker build --target build -t http2comm .
#   docker build -t http2comm .  (default: build)
# =============================================================================

FROM ubuntu:24.04 AS deps
LABEL maintainer="testillano"
LABEL testillano.http2comm_builder.description="Docker image with all dependencies to build ert_http2comm library"

WORKDIR /code/build

# ---------------------------------------------------------------------------
# Dependency versions (single source of truth)
# ---------------------------------------------------------------------------
ARG make_procs=4
ARG build_type=Release
ARG boost_ver=1.84.0
ARG nghttp2_ver=1.64.0
ARG nghttp2_asio_ver=main
ARG ert_nghttp2_ver=v1.3.0
ARG ert_logger_ver=v1.1.1
ARG ert_queuedispatcher_ver=v1.0.4
ARG jupp0r_prometheuscpp_ver=v1.3.0
ARG civetweb_civetweb_ver=v1.16
ARG ert_metrics_ver=v1.2.0

# ---------------------------------------------------------------------------
# System packages
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    wget zip tar bzip2 patch \
    make cmake g++ \
    libtool pkg-config autoconf automake \
    libssl-dev zlib1g-dev libcurl4-openssl-dev \
    doxygen graphviz \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Optimization flags (LTO + portable SIMD: x86-64-v3 = AVX2, ~2013+ CPUs)
# ---------------------------------------------------------------------------
ARG OPT_CFLAGS="-O3 -march=x86-64-v3 -flto=auto"
ARG OPT_CXXFLAGS="-O3 -march=x86-64-v3 -flto=auto"
ARG OPT_LDFLAGS="-flto=auto"
ENV CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}"

# ---------------------------------------------------------------------------
# Patches (downloaded from testillano/nghttp2 repo)
# ---------------------------------------------------------------------------
RUN set -x && \
    wget https://github.com/testillano/nghttp2/archive/${ert_nghttp2_ver}.tar.gz && \
    tar xf ${ert_nghttp2_ver}.tar.gz && \
    mv nghttp2-*/deps/patches /patches && \
    rm -rf nghttp2-* ${ert_nghttp2_ver}.tar.gz && \
    set +x

# ===========================================================================
# BOOST
# ===========================================================================
RUN set -x && \
    boost_tar=boost_$(echo ${boost_ver} | tr '.' '_').tar.gz && \
    wget -O ${boost_tar} https://boostorg.jfrog.io/artifactory/main/release/${boost_ver}/source/${boost_tar} && \
    file ${boost_tar} | grep -q gzip || \
    (rm -f ${boost_tar} && wget -O ${boost_tar} https://sourceforge.net/projects/boost/files/boost/${boost_ver}/${boost_tar}) && \
    tar xvf ${boost_tar} && cd boost*/ && \
    ./bootstrap.sh && ./b2 -j${make_procs} variant=release cxxflags="${OPT_CXXFLAGS}" linkflags="${OPT_LDFLAGS}" install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# NGHTTP2
# ===========================================================================
RUN set -x && \
    wget https://github.com/nghttp2/nghttp2/releases/download/v${nghttp2_ver}/nghttp2-${nghttp2_ver}.tar.bz2 && \
    tar xf nghttp2-${nghttp2_ver}.tar.bz2 && cd nghttp2-${nghttp2_ver}/ && \
    for patch in $(ls /patches/nghttp2/${nghttp2_ver}/*.patch 2>/dev/null); do patch -p1 < ${patch}; done && \
    CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --disable-shared --enable-python-bindings=no && make -j${make_procs} install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# NGHTTP2-ASIO
# ===========================================================================
RUN set -x && \
    wget https://github.com/nghttp2/nghttp2-asio/archive/refs/heads/${nghttp2_asio_ver}.zip && \
    unzip ${nghttp2_asio_ver}.zip && cd nghttp2-asio-${nghttp2_asio_ver} && \
    for patch in $(ls /patches/nghttp2-asio/${nghttp2_asio_ver}/*.patch 2>/dev/null); do patch -p1 < ${patch}; done && \
    autoreconf -i && automake && autoconf && \
    CFLAGS="${OPT_CFLAGS}" CXXFLAGS="${OPT_CXXFLAGS}" LDFLAGS="${OPT_LDFLAGS}" \
    ./configure --enable-shared=false && make -j${make_procs} install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_LOGGER
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/logger/archive/${ert_logger_ver}.tar.gz && \
    tar xvf ${ert_logger_ver}.tar.gz && cd logger-*/ && \
    cmake -DERT_LOGGER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_QUEUEDISPATCHER
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/queuedispatcher/archive/${ert_queuedispatcher_ver}.tar.gz && \
    tar xvf ${ert_queuedispatcher_ver}.tar.gz && cd queuedispatcher-*/ && \
    cmake -DERT_QUEUEDISPATCHER_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# ===========================================================================
# PROMETHEUS-CPP + CIVETWEB
# ===========================================================================
RUN set -x && \
    wget https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/${jupp0r_prometheuscpp_ver}.tar.gz && \
    tar xvf ${jupp0r_prometheuscpp_ver}.tar.gz && cd prometheus-cpp*/3rdparty && \
    wget https://github.com/civetweb/civetweb/archive/refs/tags/${civetweb_civetweb_ver}.tar.gz && \
    tar xvf ${civetweb_civetweb_ver}.tar.gz && mv civetweb-*/* civetweb && cd .. && \
    mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=${build_type} -DENABLE_TESTING=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON .. && \
    make -j${make_procs} && make install && \
    cd ../.. && rm -rf * && \
    set +x

# ===========================================================================
# ERT_METRICS
# ===========================================================================
RUN set -x && \
    wget https://github.com/testillano/metrics/archive/${ert_metrics_ver}.tar.gz && \
    tar xvf ${ert_metrics_ver}.tar.gz && cd metrics-*/ && \
    cmake -DERT_METRICS_BuildExamples=OFF -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf * && \
    set +x

# =============================================================================
# Stage: build (compile and install http2comm)
# =============================================================================
FROM deps AS build

ARG make_procs=4
ARG build_type=Release

COPY . /code/build/http2comm/
RUN set -x && \
    cd http2comm && \
    cmake -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON . && \
    make -j${make_procs} && make install && \
    cd .. && rm -rf http2comm && \
    set +x
