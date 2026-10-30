if(TARGET nijigenerate_vertex_drawable_editor AND TARGET nijigenerate_vertex_deformable_editor
        AND TARGET nijigenerate_mesh_editor_aggregate AND TARGET nijigenerate_runtime_core
        AND NOT TARGET nijigenerate_vertex_editor)
    add_library(nijigenerate_vertex_editor STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/editor.cpp")
    target_include_directories(nijigenerate_vertex_editor PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_vertex_editor PUBLIC cxx_std_20)
    target_sources(nijigenerate_vertex_editor PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/editor.hpp")
    target_link_libraries(nijigenerate_vertex_editor PUBLIC nijigenerate_vertex_drawable_editor
        nijigenerate_vertex_deformable_editor nijigenerate_mesh_editor_aggregate nijigenerate_runtime_core
        nijigenerate_node_editor)
    # The actual root declares node_editor after this aggregate.
    function(ng_require_vertex_editor_providers)
        foreach(provider IN ITEMS nijigenerate_vertex_drawable_editor nijigenerate_vertex_deformable_editor
                nijigenerate_mesh_editor_aggregate nijigenerate_runtime_core nijigenerate_node_editor)
            if(NOT TARGET "${provider}")
                message(FATAL_ERROR "VertexMeshEditor requires actual provider ${provider}")
            endif()
        endforeach()
    endfunction()
    cmake_language(DEFER CALL ng_require_vertex_editor_providers)
    if(MSVC)
        target_compile_options(nijigenerate_vertex_editor PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_vertex_editor PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
