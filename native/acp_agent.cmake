if(TARGET nijigenerate_acp_stdio AND TARGET nijigenerate_acp_protocol
        AND TARGET nijigenerate_acp_types AND NOT TARGET nijigenerate_acp_agent)
    add_library(nijigenerate_acp_agent STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/agent.cpp")
    target_sources(nijigenerate_acp_agent PUBLIC FILE_SET acp_agent_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/agent.hpp")
    target_include_directories(nijigenerate_acp_agent PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_acp_agent PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_acp_agent PUBLIC
        nijigenerate_acp_stdio nijigenerate_acp_protocol nijigenerate_acp_types)
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_acp_agent PUBLIC nijigenerate_native_options)
    endif()
endif()
