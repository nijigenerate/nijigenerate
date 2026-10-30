if(TARGET nijigenerate_runtime_core)
    set(output_widget_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_internal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_view_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_view_update.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_tree.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_icon.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_views.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/dragdrop_node_payload.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${output_widget_sources})
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET output_widget_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_tree.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/output_internal.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/retained_string_map.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_controller nijigenerate_texture_widget
        nijigenerate_dragdrop nijigenerate_button nijigenerate_dummy nijigenerate_label nijigenerate_tooltip
        nijigenerate_dialog_queue nijigenerate_inspector_common)
    if(MSVC)
        set_source_files_properties(${output_widget_sources} PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties(${output_widget_sources} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
