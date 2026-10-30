if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET puppet_tool_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool_handlers.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool_table.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool_map_metadata.hpp")
    if(TARGET nijigenerate_autorig_framework)
        target_sources(nijigenerate_runtime_core PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/tool.cpp")
        target_link_libraries(nijigenerate_runtime_core PRIVATE nijigenerate_autorig_framework)
    else()
        message(STATUS "Puppet tool commands require the real AutoRig framework provider")
    endif()
endif()
