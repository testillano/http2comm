add_library (${ERT_HTTP2COMM_TARGET_NAME} STATIC
        ${CMAKE_CURRENT_LIST_DIR}/Http2Client.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Http2Connection.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Http2Server.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Http2Headers.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Stream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/URLFunctions.cpp
)

target_include_directories(${ERT_HTTP2COMM_TARGET_NAME}
  PUBLIC ${ERT_HTTP2COMM_INCLUDE_BUILD_DIR}
)

target_link_libraries(${ERT_HTTP2COMM_TARGET_NAME}
        ert_logger
        ert_queuedispatcher
        ert_metrics
)

install(TARGETS ${ERT_HTTP2COMM_TARGET_NAME}
        ARCHIVE DESTINATION lib/ert)
