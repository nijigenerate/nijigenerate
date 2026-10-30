if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/edit.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET puppet_edit_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/edit.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/edit_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/edit_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/puppet/edit_table.hpp")
endif()
