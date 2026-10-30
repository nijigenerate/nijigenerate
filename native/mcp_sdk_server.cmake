# Production MCP SDK closure. Regex compatibility remains an explicit provider requirement.
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/schema.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/tools.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/resources.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/regex_resource_subset.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/prompts.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/server.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_acp_stdio)
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/schema.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/providers/mcp/schema.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
