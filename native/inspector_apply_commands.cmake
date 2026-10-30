get_filename_component(_inspector_apply_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(TARGET nijigenerate_runtime_core)
    set(_inspector_apply_target nijigenerate_runtime_core)
else()
    set(_inspector_apply_target nijigenerate_core)
endif()
target_sources(${_inspector_apply_target} PUBLIC
    FILE_SET inspector_apply_headers TYPE HEADERS
    BASE_DIRS "${_inspector_apply_root}/native/src"
    FILES "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_initialization.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_actions.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_command_bodies.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_format.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_table.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/node_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/drawable_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/camera_command.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/composite_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/part_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/meshgroup_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/griddeform_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/path_commands.hpp"
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/simplephysics_commands.hpp")
if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_actions.cpp")
elseif(NIJIGENERATE_RUNTIME_PROBE)
    target_sources(nijigenerate_core PRIVATE
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_actions.cpp")
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    target_sources(nijigenerate_core PRIVATE
        "${_inspector_apply_root}/native/src/nijigenerate/commands/inspector/apply_node_command_declarations.cpp")
    target_link_libraries(nijigenerate_core PUBLIC nicxlive::nicxlive)
endif()
