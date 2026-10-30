option(NIJIGENERATE_MCP_LOG "Enable source MCP_LOG diagnostics" OFF)
if(NIJIGENERATE_IMGUI AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_notification
    AND TARGET nijigenerate_button AND TARGET nijigenerate_phobos_stdio
    AND TARGET nijigenerate_mcp_queue AND TARGET nijigenerate_tasks)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/auth.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/auth_wait.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET mcp_auth_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/auth.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_notification nijigenerate_button
        nijigenerate_phobos_stdio nijigenerate_mcp_queue)
    if(NIJIGENERATE_MCP_LOG)
        target_compile_definitions(nijigenerate_runtime_core PRIVATE NIJIGENERATE_MCP_LOG=1)
    endif()
endif()
