if(TARGET nijigenerate_mesh_editor_tools AND NOT TARGET nijigenerate_vertex_actions)
    # The actual vertex.cpp source is already compiled into mesh_editor_tools.
    # Expose the source module without creating a second action implementation.
    add_library(nijigenerate_vertex_actions INTERFACE)
    target_include_directories(nijigenerate_vertex_actions INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_vertex_actions INTERFACE cxx_std_20)
    target_sources(nijigenerate_vertex_actions INTERFACE FILE_SET vertex_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/vertex.hpp")
    target_link_libraries(nijigenerate_vertex_actions INTERFACE nijigenerate_mesh_editor_tools)
endif()
