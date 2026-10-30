# Register the public module only after all concrete action providers exist.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_actions_depthbone
        AND TARGET nijigenerate_mesh_editor_tools AND TARGET nijigenerate_depth_editor_commands
        AND TARGET nijigenerate_parameter_actions)
    add_library(nijigenerate_actions INTERFACE)
    target_include_directories(nijigenerate_actions INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_actions INTERFACE cxx_std_20)
    target_sources(nijigenerate_actions INTERFACE
        FILE_SET actions_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/package.hpp")
    target_link_libraries(nijigenerate_actions INTERFACE
        nijigenerate_runtime_core nijigenerate_actions_depthbone
        nijigenerate_mesh_editor_tools nijigenerate_depth_editor_commands
        nijigenerate_parameter_actions)
endif()
