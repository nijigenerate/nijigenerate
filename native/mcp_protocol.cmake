if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/mcp_protocol.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET mcp_protocol_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/mcp_protocol.hpp")
endif()
