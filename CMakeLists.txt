############
# Versions #
############
cmake_minimum_required (VERSION 3.5.1)
project(ert_http2comm VERSION 0.0.2 LANGUAGES CXX)

set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MAIN_PROJECT ON)
endif()

###########
# Modules #
###########
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/")

###########################
# Options & Configuration #
###########################
set(ERT_HTTP2COMM_TARGET_NAME       ${PROJECT_NAME})
set(ERT_HTTP2COMM_INCLUDE_BUILD_DIR "${PROJECT_SOURCE_DIR}/include/ert")

################
# Dependencies #
################
include(FetchContent)

FetchContent_Declare(ert_logger
  GIT_REPOSITORY https://github.com/testillano/logger.git
  GIT_TAG v1.0.2)
message(STATUS  "Fetching ert_logger ... this may take a while")
FetchContent_GetProperties(ert_logger)

if(NOT ert_logger_POPULATED)
  FetchContent_Populate(ert_logger)
  add_subdirectory(${ert_logger_SOURCE_DIR} ${ert_logger_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(tatsuhiro_nghttp2
  GIT_REPOSITORY https://github.com/nghttp2/nghttp2
  GIT_TAG v1.43.0)
message(STATUS  "Fetching tatsuhiro_nghttp2 ... this may take a while")
FetchContent_GetProperties(tatsuhiro_nghttp2)

if(NOT tatsuhiro_nghttp2)
  FetchContent_Populate(tatsuhiro_nghttp2)
  add_subdirectory(${tatsuhiro_nghttp2_SOURCE_DIR} ${tatsuhiro_nghttp2_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

##############
# Build type #
##############
include(build_type)
set_cmake_build_type()
message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

#########
# Flags #
#########
add_compile_options(
    "-Wall"
    "-Werror"
    $<$<CONFIG:Debug>:--coverage>
)

link_libraries(
    $<$<CONFIG:Debug>:--coverage>
)

############
# Includes #
############
#add_library(${ERT_HTTP2COMM_TARGET_NAME} INTERFACE)
#add_library(${PROJECT_NAME}::${ERT_HTTP2COMM_TARGET_NAME} ALIAS ${ERT_HTTP2COMM_TARGET_NAME})
#target_include_directories(
#    ${ERT_HTTP2COMM_TARGET_NAME}
#    INTERFACE
#    $<BUILD_INTERFACE:${ERT_HTTP2COMM_INCLUDE_BUILD_DIR}>
#    $<INSTALL_INTERFACE:include>
#)

##################
# Subdirectories #
##################
add_subdirectory( src )

