if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_mesh_editor_operations_impl AND
        TARGET nijigenerate_model_drawable_editor AND TARGET nijigenerate_spline AND
        TARGET nijigenerate_viewport_base_primitives AND NOT TARGET nijigenerate_mesh_editor_tools)
    # Action, Tool and OneTimeDeform mutually reference their actual editor bodies.
    add_library(nijigenerate_mesh_editor_tools STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesh.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/package.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesheditor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/vertex.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/select.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/point.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/brush.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/lasso.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform_scope.cpp")
    target_include_directories(nijigenerate_mesh_editor_tools PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_mesh_editor_tools PUBLIC
        FILE_SET mesh_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/mesh.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/base.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/enums.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/detail/source_c_string_contract.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/select.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/brush.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform_scope.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.tpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/catalog.hpp")
    target_compile_features(nijigenerate_mesh_editor_tools PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_mesh_editor_tools PUBLIC nijigenerate_runtime_core
        nijigenerate_mesh_editor_operations_impl nijigenerate_model_drawable_editor
        nijigenerate_mesh_editor_aggregate nijigenerate_spline nijigenerate_debug
        nijigenerate_viewport_base_primitives nijigenerate_viewport_common
        nijigenerate_input nijigenerate_statusbar nijigenerate_button nijigenerate_tooltip
        nijigenerate_mesh_brushes nijigenerate_brushstate nijigenerate_texture_widget)
    target_link_libraries(nijigenerate_model_drawable_editor PUBLIC nijigenerate_mesh_editor_tools)
    target_link_libraries(nijigenerate_mesh_editor_operations_impl PUBLIC nijigenerate_mesh_editor_tools)
    target_link_libraries(nijigenerate_mesh_editor_aggregate PUBLIC nijigenerate_mesh_editor_tools)
    if(MSVC)
        target_compile_options(nijigenerate_mesh_editor_tools PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_mesh_editor_tools PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
