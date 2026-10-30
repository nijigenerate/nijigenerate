if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_spline AND
        TARGET nijigenerate_viewport_base_primitives AND TARGET nijigenerate_viewport_common AND
        TARGET nijigenerate_mesh_editor_operations_impl AND NOT TARGET nijigenerate_vertex_deformable_editor)
    add_library(nijigenerate_vertex_deformable_editor STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/deformable.cpp")
    target_include_directories(nijigenerate_vertex_deformable_editor PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_vertex_deformable_editor PUBLIC cxx_std_20)
    target_sources(nijigenerate_vertex_deformable_editor PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/deformable.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/deformable.tpp")
    target_link_libraries(nijigenerate_vertex_deformable_editor PUBLIC nijigenerate_runtime_core
        nijigenerate_spline nijigenerate_debug nijigenerate_viewport_base_primitives
        nijigenerate_viewport_common nijigenerate_mesh_editor_operations_impl)
    if(MSVC)
        target_compile_options(nijigenerate_vertex_deformable_editor PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_vertex_deformable_editor PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
