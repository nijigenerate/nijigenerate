if(TARGET nijigenerate_texture_widget AND TARGET nijigenerate_dragdrop AND TARGET nijigenerate_label)
    target_sources(nijigenerate_dragdrop PRIVATE src/nijigenerate/widgets/dragdrop_nodes.cpp)
    target_link_libraries(nijigenerate_dragdrop PRIVATE nijigenerate_texture_widget nijigenerate_label)
    if(BUILD_TESTING)
        add_executable(nijigenerate_dragdrop_nodes_imgui_probe tests/dragdrop_nodes_imgui_probe.cpp)
        target_link_libraries(nijigenerate_dragdrop_nodes_imgui_probe PRIVATE
            nijigenerate_dragdrop nijigenerate_texture_widget)
        add_executable(nijigenerate_dragdrop_nodes_recording_probe tests/dragdrop_nodes_recording_probe.cpp
            src/nijigenerate/widgets/dragdrop_nodes.cpp)
        target_include_directories(nijigenerate_dragdrop_nodes_recording_probe PRIVATE tests/dragdrop_nodes_recording src)
        target_link_libraries(nijigenerate_dragdrop_nodes_recording_probe PRIVATE nijigenerate_native_options)
        add_dependencies(nijigenerate_dragdrop_project nijigenerate_dragdrop_nodes_imgui_probe
            nijigenerate_dragdrop_nodes_recording_probe)
        if(WIN32)
            get_target_property(nodes_json_type Boost::json TYPE)
            if(nodes_json_type STREQUAL "SHARED_LIBRARY")
                add_custom_command(TARGET nijigenerate_dragdrop_nodes_imgui_probe POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "$<TARGET_FILE:Boost::json>" "$<TARGET_FILE_DIR:nijigenerate_dragdrop_nodes_imgui_probe>")
            endif()
        endif()
        add_test(NAME native_dragdrop_nodes_imgui_contract COMMAND nijigenerate_dragdrop_nodes_imgui_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
            add_test(NAME native_dragdrop_nodes_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_button_color.py"
                    --widget dragdrop_nodes --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_dragdrop_nodes_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/dragdrop-nodes-differential/$<CONFIG>")
        endif()
    endif()
endif()
