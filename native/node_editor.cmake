if(TARGET nijigenerate_mesh_editor_operations_impl AND TARGET nijigenerate_mesh_editor_tools
        AND TARGET nijigenerate_core AND TARGET nijigenerate_spline AND TARGET nijigenerate_debug
        AND NOT TARGET nijigenerate_node_editor)
    add_library(nijigenerate_node_editor INTERFACE)
    target_sources(nijigenerate_node_editor INTERFACE
        FILE_SET node_editor_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/node.hpp")
    target_include_directories(nijigenerate_node_editor INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_node_editor INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_node_editor INTERFACE
        nijigenerate_mesh_editor_operations_impl nijigenerate_mesh_editor_tools
        nijigenerate_core nijigenerate_spline nijigenerate_debug)
    if(MSVC)
        target_compile_options(nijigenerate_node_editor INTERFACE /fp:strict)
    else()
        target_compile_options(nijigenerate_node_editor INTERFACE -ffp-contract=off -fno-fast-math)
    endif()
endif()
