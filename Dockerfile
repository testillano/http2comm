ARG base_tag=latest
FROM testillano/http2comm_builder:${base_tag}
MAINTAINER testillano

LABEL testillano.http2comm.description="ert_http2comm library image"

WORKDIR /code/build

ARG make_procs=4
ARG build_type=Release

# ert_http2comm
COPY . /code/build/http2comm/
RUN set -x && \
    cd http2comm && cmake -DCMAKE_BUILD_TYPE=${build_type} . && make -j${make_procs} && make install && \
    cd .. && rm -rf http2comm && \
    set +x
