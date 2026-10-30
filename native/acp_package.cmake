if(TARGET nijigenerate_acp_protocol AND TARGET nijigenerate_acp_stdio
    AND TARGET nijigenerate_acp_types
    AND TARGET nijigenerate_acp_agent AND TARGET nijigenerate_acp_client
    AND NOT TARGET nijigenerate_acp)
    add_library(nijigenerate_acp INTERFACE)
    target_include_directories(nijigenerate_acp INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_acp INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/package.hpp")
    target_link_libraries(nijigenerate_acp INTERFACE nijigenerate_acp_protocol
        nijigenerate_acp_types nijigenerate_acp_stdio nijigenerate_acp_agent nijigenerate_acp_client)
endif()
