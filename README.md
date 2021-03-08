# C++ HTTP/2 wrapper library - WORK IN PROGRESS

This library is based on @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2).
It offers a quick way to instantiate a client or server and define their virtual methods to
process the requests and answers properly.

## Build project with docker

### Build image

     > cd ./docker/http2comm_build
     > tag=$(git tag | grep -E '^v[0-9.]+$' | tail -n -1 | cut -c2-) # remove leading 'v'
     > img=testillano/http2comm_build:${tag}
     > docker build --rm -t ${img} .
     > cd - >/dev/null # return to project root

#### Download from docker hub

This image is already available at docker hub for every repository tag, and also for master as `latest`:

     > docker pull testillano/http2comm_build

### Build library

With the previous image, we can now build the library:

     > envs="-e build_type=Release -e make_procs=$(grep processor /proc/cpuinfo -c)"
     > docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code ${img}

## Build project natively

This is a cmake-based building library, so you may install cmake:

     > sudo apt-get install cmake

And then generate the makefiles from project root directory:

     > cmake .

You could specify type of build, 'Debug' or 'Release', for example:

     > cmake -DCMAKE_BUILD_TYPE=Debug .
     > cmake -DCMAKE_BUILD_TYPE=Release .

You could also change the compilers used:

     > cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++     -DCMAKE_C_COMPILER=/usr/bin/gcc
     or
     > cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang

### Requirements

Check the requirements installed at building dockerfile: `./docker/http2comm_build/Dockerfile`.

### Build

     > make

### Clean

     > make clean

### Documentation

     > make doc


     > cd docs/doxygen
     > tree -L 1

     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man

### Install

     > sudo make install

Optionally you could specify another prefix for installation:

     > cmake -DMY_OWN_INSTALL_PREFIX=$HOME/mylibs/ert_http2comm
     > make install

### Uninstall

     > cat install_manifest.txt | sudo xargs rm

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

Please, execute `astyle` formatting before any pull request:

    docker pull frankwolf/astyle
    docker run -it --rm -v $PWD:/data frankwolf/astyle include/ert/http2comm/*.hpp src/*.cpp
