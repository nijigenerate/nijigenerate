# Reuse the actual command registry, scope providers and MCP blocking state.
target_sources(nijigenerate_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_execution.cpp")
target_link_libraries(nijigenerate_core PUBLIC nijigenerate_mcp_queue)
