[TOC]

# C++ HTTP/2 wrapper library - WORK IN PROGRESS

This library is based on @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2).
It offers a quick way to instantiate a client or server and define their virtual methods to
process the requests and answers properly.

## Build project with docker

### Builder image

This image is already available at `docker hub` for every repository `tag`, and also for master as `latest`. Anyway, to create it, just type the following:

```bash
$ make_procs=$(grep processor /proc/cpuinfo -c)
$ docker build --rm --build-arg make_procs=${make_procs} -t testillano/http2comm_build .
```

### Usage

To run compilation over this image, just run with `docker`. The `entrypoint` will search for `CMakeLists.txt` file at project root (i.e. mounted on working directory `/code`) to generate `makefiles` and then, builds the source code with `make`. There are two available environment variables: `BUILD_TYPE` (for `cmake`) and `MAKE_PROCS` (for `make`) which are inherited from base image (`nghttp2_build`):

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/http2comm_build
```

You could generate documentation understanding the builder script behind ([nghttp2 build entrypoint](https://github.com/testillano/nghttp2_build/blob/master/deps/build.sh)):

```bash
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         testillano/http2comm_build "" doc
```

## Build project natively

This is a cmake-based building library, so you may install cmake:

```bash
$ sudo apt-get install cmake
```

And then generate the makefiles from project root directory:

```bash
$ cmake .
```

You could specify type of build, 'Debug' or 'Release', for example:

```bash
$ cmake -DCMAKE_BUILD_TYPE=Debug .
$ cmake -DCMAKE_BUILD_TYPE=Release .
```

You could also change the compilers used:

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++     -DCMAKE_C_COMPILER=/usr/bin/gcc
```
or

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
```

### Requirements

Check the requirements described at building `dockerfile` (`./docker/http2comm_build/Dockerfile`) as well as all the ascendant docker images which are inherited:

```
http2comm_build (./docker/http2comm_build/Dockerfile)
   |
nghttp2_build (https://github.com/testillano/nghttp2_build)
```

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

```bash
$ cd docs/doxygen
$ tree -L 1
     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man
```

### Install

```bash
$ sudo make install
```

Optionally you could specify another prefix for installation:

```bash
$ cmake -DMY_OWN_INSTALL_PREFIX=$HOME/mylibs/ert_http2comm
$ make install
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
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -it --rm -v $PWD:/data frankwolf/astyle ${sources}
```

