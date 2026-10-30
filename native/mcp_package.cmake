if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_mcp_queue
    AND NOT TARGET nijigenerate_mcp)
    add_library(nijigenerate_mcp INTERFACE)
    target_sources(nijigenerate_mcp INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/package.hpp")
    target_include_directories(nijigenerate_mcp INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_mcp INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_mcp INTERFACE nijigenerate_runtime_core nijigenerate_mcp_queue)
endif()
