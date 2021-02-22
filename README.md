# C++ HTTP/2 wrapper library

This library is based on @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2).
It offers a quick way to instantiate a client or server and define their virtual methods to
process the requests and answers properly.

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
  GIT_TAG v1.0.0)

FetchContent_GetProperties(ert_http2comm)
if(NOT ert_json_POPULATED)
  FetchContent_Populate(ert_http2comm)
  add_subdirectory(${ert_http2comm_SOURCE_DIR} ${ert_http2comm_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(foo PRIVATE ert_http2comm::ert_http2comm)
```

### Build

    mkdir .release
    cd .release
    cmake ..
    make

### Contributing

Please, execute `astyle` formatting before any pull request:

    docker pull frankwolf/astyle
    docker run -it --rm -v $PWD:/data frankwolf/astyle include/ert/http2comm/*.hpp src/*.cpp

