if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/resource_listing.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET mcp_resource_listing_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/resource_listing.hpp")
endif()
