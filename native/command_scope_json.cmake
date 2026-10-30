# Compile the actual source JSON body with its one ordinary value provider.
if(TARGET Boost::json AND TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_command_scope_json STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_scope.cpp")
    target_link_libraries(nijigenerate_command_scope_json PUBLIC
        nijigenerate_core nijigenerate_runtime_core Boost::json)
    target_sources(nijigenerate_command_scope_json PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_scope.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_scope_command.hpp")
endif()
