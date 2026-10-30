if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_native_options AND TARGET nicxlive::nicxlive
        AND TARGET nijigenerate_vertex_viewport AND TARGET nijigenerate_viewport_mesh
        AND NOT TARGET nijigenerate_vertex_define_commands)
    add_library(nijigenerate_vertex_define_commands STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh_constructors.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh_runnable.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh_run.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh_preflight.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh_registry.cpp")
    target_include_directories(nijigenerate_vertex_define_commands PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_vertex_define_commands PUBLIC
        FILE_SET vertex_define_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/vertex/define_mesh.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp")
    target_compile_features(nijigenerate_vertex_define_commands PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_vertex_define_commands
        PUBLIC nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive
        PRIVATE nijigenerate_vertex_viewport nijigenerate_viewport_mesh)
endif()
