# One real task/event producer is also usable by queue-only source consumers.
if(TARGET nijigenerate_tasks AND NOT TARGET nijigenerate_vibe_task_runtime)
    add_library(nijigenerate_vibe_task_runtime STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_event_runtime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_task_runtime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_auth_time.hpp")
    target_compile_features(nijigenerate_vibe_task_runtime PUBLIC cxx_std_20)
    target_include_directories(nijigenerate_vibe_task_runtime PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_vibe_task_runtime PUBLIC FILE_SET vibe_task_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/auth_wait.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_task_runtime.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_event_runtime.hpp")
    target_link_libraries(nijigenerate_vibe_task_runtime PUBLIC
        nijigenerate_tasks nijigenerate_phobos_stdio Boost::headers)
    if(WIN32)
        target_link_libraries(nijigenerate_vibe_task_runtime PRIVATE ws2_32 mswsock user32)
    endif()
endif()

# Real request-fiber HTTP server; the SDK and auth providers share runtime_core.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_vibe_task_runtime)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_transport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_request_header.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_status.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_chunk_writer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_error_body.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_auth_time.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_route_match.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_request_url.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_request_loop.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_source_log.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET mcp_http_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/http_transport.hpp")
    target_link_libraries(nijigenerate_runtime_core
        PUBLIC nijigenerate_acp_stdio nijigenerate_vibe_task_runtime)
    if(NIJIGENERATE_MCP_LOG)
        target_compile_definitions(nijigenerate_runtime_core PRIVATE NIJIGENERATE_MCP_LOG=1)
    endif()
    if(WIN32)
        target_link_libraries(nijigenerate_runtime_core PRIVATE ws2_32 mswsock)
    endif()
endif()
