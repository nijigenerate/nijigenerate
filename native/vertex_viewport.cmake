if(TARGET nijigenerate_vertex_editor AND TARGET nijigenerate_automesh_optimum
        AND TARGET nijigenerate_automesh
        AND TARGET nijigenerate_automesh_contours AND TARGET nijigenerate_automesh_grid
        AND TARGET nijigenerate_automesh_skeleton AND TARGET nijigenerate_tasks
        AND TARGET nijigenerate_main_viewport AND TARGET nijigenerate_model_editor_completion
        AND TARGET nijigenerate_mesh_editor_tools AND TARGET nijigenerate_dialogs
        AND NOT TARGET nijigenerate_vertex_viewport)
    add_library(nijigenerate_vertex_viewport STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/package.cpp")
    target_include_directories(nijigenerate_vertex_viewport PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_vertex_viewport PUBLIC cxx_std_20)
    target_sources(nijigenerate_vertex_viewport PUBLIC
        FILE_SET vertex_viewport_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/package.hpp")
    target_link_libraries(nijigenerate_vertex_viewport PUBLIC nijigenerate_vertex_editor
        nijigenerate_automesh
        nijigenerate_runtime_core nijigenerate_automesh_optimum nijigenerate_automesh_contours
        nijigenerate_automesh_grid nijigenerate_automesh_skeleton nijigenerate_tasks
        nijigenerate_button nijigenerate_tooltip nijigenerate_dummy nijigenerate_dialogs)
    if(MSVC)
        target_compile_options(nijigenerate_vertex_viewport PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_vertex_viewport PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
