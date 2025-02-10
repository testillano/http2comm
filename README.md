# C++ HTTP/2 wrapper library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/http2comm.svg)](https://codedocs.xyz/testillano/http2comm/index.html)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/http2comm/graphs/commit-activity)
[![Main project workflow](https://github.com/testillano/http2comm/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/http2comm/actions/workflows/ci.yml)
[![Docker Pulls](https://img.shields.io/docker/pulls/testillano/http2comm.svg)](https://github.com/testillano/http2comm/pkgs/container/http2comm)

This library is based on @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2).
It offers a quick way to instantiate a client or server and define their virtual methods to
process the requests and answers properly.

You could check an example of use at [h2agent](https://github.com/testillano/h2agent) project, where the server side capability is used to mock an HTTP/2 service within a testing ecosystem.

## Project image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/http2comm:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$> ./build.sh --project-image
```

This image is built with `./Dockerfile`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

## Usage

To run compilation over this image, just run with `docker`. The `entrypoint` (check it at `./deps/build.sh`) will fall back from `cmake` (looking for `CMakeLists.txt` file at project root, i.e. mounted on working directory `/code` to generate makefiles) to `make`, in order to build your source code. There are two available environment variables used by the builder script of this image: `BUILD_TYPE` (for `cmake`) and `MAKE_PROCS` (for `make`):

```bash
$> envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$> docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/http2comm:<tag>
```

## Build project with docker

### Builder image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$> docker pull ghcr.io/testillano/http2comm_builder:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$> ./build.sh --builder-image
```

This image is built with `./Dockerfile.build`.
Both `ubuntu` and `alpine` base images are supported, but the official image uploaded is the one based in `ubuntu`.

### Usage

Builder image is used to build the project library. To run compilation over this image, again, just run with `docker`:

```bash
$> envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$> docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/http2comm_builder:<tag>
```

You could generate documentation passing extra arguments to the [entry point](https://github.com/testillano/nghttp2/blob/master/deps/build.sh) behind:

```bash
$> docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
          ghcr.io/testillano/http2comm_builder::<tag>-build "" doc
```

You could also build the library using the script `./build.sh` located at project root:


```bash
$> ./build.sh --project
```

## Build project natively

This is a cmake-based building library, so you may install cmake:

```bash
$> sudo apt-get install cmake
```

And then generate the makefiles from project root directory:

```bash
$> cmake .
```

You could specify type of build, 'Debug' or 'Release', for example:

```bash
$> cmake -DCMAKE_BUILD_TYPE=Debug .
$> cmake -DCMAKE_BUILD_TYPE=Release .
```

You could also change the compilers used:

```bash
$> cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_C_COMPILER=/usr/bin/gcc
```
or

```bash
$> cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
```

### Requirements

Check the requirements described at building `dockerfile` (`./Dockerfile.build`) as well as all the ascendant docker images which are inherited:

```
http2comm builder (./Dockerfile.build)
   |
nghttp2 (https://github.com/testillano/nghttp2)
```

### Build

```bash
$> make
```

### Clean

```bash
$> make clean
```

### Documentation

```bash
$> make doc
```

```bash
$> cd docs/doxygen
$> tree -L 1
     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man
```

### Install

```bash
$> sudo make install
```

Optionally you could specify another prefix for installation:

```bash
$> cmake -DMY_OWN_INSTALL_PREFIX=$HOME/mylibs/ert_http2comm
$> make install
```

### Uninstall

```bash
$> cat install_manifest.txt | sudo xargs rm
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

Since CMake v3.11,
[FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) can be used to automatically download the repository as a dependency at configure type.

Example:

```cmake
include(FetchContent)

FetchContent_Declare(ert_http2comm
  GIT_REPOSITORY https://github.com/testillano/http2comm.git
  GIT_TAG vx.y.z)

FetchContent_GetProperties(ert_http2comm)
if(NOT ert_json_POPULATED)
  FetchContent_Populate(ert_http2comm)
  add_subdirectory(${ert_http2comm_SOURCE_DIR} ${ert_http2comm_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(foo PRIVATE ert_http2comm::ert_http2comm)
```

## Contributing

Please, execute `astyle` formatting (using [frankwolf image](https://hub.docker.com/r/frankwolf/astyle)) before any pull request:

```bash
$> sources=$(find . -name "*.hpp" -o -name "*.cpp")
$> docker run -i --rm -v $PWD:/data frankwolf/astyle ${sources}
```
