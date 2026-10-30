if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET mcp_server_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/thread_exception.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/scope_exit.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/uri_exception.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_schema.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_tools.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_prompts.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_resources.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_resource_registration.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_lifecycle.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_mcp_queue)
    if(MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_schema.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/server_tools.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    endif()
endif()
