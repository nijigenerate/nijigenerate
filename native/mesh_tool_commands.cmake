if(TARGET nijigenerate_mesh_editor_aggregate AND TARGET nijigenerate_mesh_editor_tools
        AND TARGET nijigenerate_model_editor_completion AND TARGET nijigenerate_vertex_viewport
        AND NOT TARGET nijigenerate_mesh_tool_commands)
    add_library(nijigenerate_mesh_tool_commands INTERFACE)
    target_include_directories(nijigenerate_mesh_tool_commands INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_mesh_tool_commands INTERFACE cxx_std_20)
    # tool.cpp is already owned by the aggregate; reuse that archive once.
    target_link_libraries(nijigenerate_mesh_tool_commands INTERFACE
        nijigenerate_mesh_editor_aggregate nijigenerate_mesh_editor_tools
        nijigenerate_model_editor_completion nijigenerate_vertex_viewport nijigenerate_core)
endif()
