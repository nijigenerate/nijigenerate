if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET mcp_helpers_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/helpers.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/helpers_result.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/helpers_payload.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/detail/helper_number.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/detail/helper_value_string.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/helpers_context.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/helpers_result.cpp")
endif()
