if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    add_library(nijigenerate_simplephysics_commands STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics.cpp")
    target_sources(nijigenerate_simplephysics_commands PUBLIC
        FILE_SET simplephysics_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_parameter_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_config_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_complete.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_float_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_constructor_programs.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/detail/simplephysics_constructor_text.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_float_mixin.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_action.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_alias.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/detail/simplephysics_type_origins.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/detail/simplephysics_notification.hpp")
    target_include_directories(nijigenerate_simplephysics_commands PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_simplephysics_commands PUBLIC
        nijigenerate_native_options nicxlive::nicxlive)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_simplephysics_commands)
    target_link_libraries(nijigenerate_simplephysics_commands PUBLIC nijigenerate_runtime_core)
    add_custom_target(nijigenerate_simplephysics_commands_project)
    add_dependencies(nijigenerate_simplephysics_commands_project nijigenerate_simplephysics_commands)
endif()
