FROM testillano/nghttp2_build:latest

WORKDIR /code/build

# ert_logger
RUN set -x && \
    wget https://github.com/testillano/logger/archive/v1.0.5.tar.gz && tar xvf v1.0.5.tar.gz && cd logger-*/ && \
    cmake . -DERT_LOGGER_BuildExamples=OFF && make && make install && \
    cd .. && rm -rf * && \
    set +x
