add_library (${ERT_HTTP2COMM_TARGET_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/Http2Client.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Http2Connection.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Http2Server.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ResponseHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Stream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/URLFunctions.cpp
)

target_include_directories (${ERT_HTTP2COMM_TARGET_NAME}
        PUBLIC ${ERT_LOGGER_INCLUDE_BUILD_DIR}
               ${ERT_HTTP2COMM_INCLUDE_BUILD_DIR}
               ${tatsuhiro_nghttp2_SOURCE_DIR}/src/includes
               ${tatsuhiro_nghttp2_SOURCE_DIR}/lib/includes
               ${tatsuhiro_nghttp2_BINARY_DIR}/lib/includes
)

target_link_libraries(${ERT_HTTP2COMM_TARGET_NAME}
        ert_logger
)

