# C++ HTTP/2 wrapper library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/http2comm.svg)](https://codedocs.xyz/testillano/http2comm/index.html)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/http2comm/graphs/commit-activity)
[![Main project workflow](https://github.com/testillano/http2comm/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/http2comm/actions/workflows/ci.yml)
[![Container](https://img.shields.io/badge/Container-ghcr.io-blue.svg)](https://github.com/testillano/http2comm/pkgs/container/http2comm)

This library is based on @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2).
It offers a quick way to instantiate a client or server and define their virtual methods to
process the requests and answers properly.

You could check an example of use at [h2agent](https://github.com/testillano/h2agent) project, where the server side capability is used to mock an HTTP/2 service within a testing ecosystem.

## Build with Docker

The project uses a single multi-stage `Dockerfile` with all dependencies built from source. No external builder images are required -- the build is fully self-contained from `ubuntu:24.04`.

### Docker targets

| Target | Produces | Description |
|--------|----------|-------------|
| `deps` | `http2comm_builder` | Toolchain + all libraries installed. |
| `build` | `http2comm` | deps + http2comm compiled and installed. |

### Quick build

```bash
$ ./build.sh                    # builds everything (deps + library)
$ ./build.sh --builder          # builds only deps stage
```

Or directly with Docker:

```bash
$ docker build --target build -t http2comm .          # full build
$ docker build --target deps -t http2comm_builder .   # deps only
```

### Pulling pre-built images

Images are available at `github container registry` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/http2comm:<tag>
```

### Overriding dependency versions

All dependency versions are declared as `ARG` at the top of the `Dockerfile`. Override any version at build time:

```bash
$ docker build --build-arg ert_metrics_ver=v1.3.0 -t http2comm .
$ ert_metrics_ver=v1.3.0 ./build.sh
```

## Build natively

This is a cmake-based library. Once dependencies are installed (see `Dockerfile` ARGs and RUN steps for the full list):

```bash
$ cmake . && make -j$(nproc)
```

You could specify type of build:

```bash
$ cmake -DCMAKE_BUILD_TYPE=Debug .
$ cmake -DCMAKE_BUILD_TYPE=Release .
```

### Requirements

All dependencies and their versions are documented in the `Dockerfile` itself (the `ARG` declarations at the top and the `RUN` steps that install them).

### Build

```bash
$ make
```

### Clean

```bash
$ make clean
```

### Documentation

```bash
$ make doc
```

### Install

```bash
$ sudo make install
```

### Uninstall

```bash
$ cat install_manifest.txt | sudo xargs rm
```

## Integration

### CMake

#### Embedded

##### Replication

To embed the library directly into an existing CMake project, place the entire source tree in a subdirectory and call `add_subdirectory()` in your `CMakeLists.txt` file:

```cmake
add_subdirectory(ert_http2comm)
...
add_library(foo ...)
...
target_link_libraries(foo PRIVATE ert_http2comm::ert_http2comm)
```

##### FetchContent

Since CMake v3.11, [FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) can be used to automatically download the repository as a dependency at configure time.

Example:

```cmake
include(FetchContent)

FetchContent_Declare(ert_http2comm
  GIT_REPOSITORY https://github.com/testillano/http2comm.git
  GIT_TAG vx.y.z)

FetchContent_GetProperties(ert_http2comm)
if(NOT ert_http2comm_POPULATED)
  FetchContent_Populate(ert_http2comm)
  add_subdirectory(${ert_http2comm_SOURCE_DIR} ${ert_http2comm_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(foo PRIVATE ert_http2comm::ert_http2comm)
```

## Note

Downstream applications (h2agent) no longer inherit from this image directly. They install http2comm from source in their own multi-stage Dockerfile with pinned versions.

## Contributing

Please, execute `astyle` formatting before any pull request:

```bash
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -i --rm -v $PWD:/data frankwolf/astyle ${sources}
```
