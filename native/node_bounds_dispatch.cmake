if(BUILD_TESTING AND TARGET nicxlive::nicxlive)
    add_executable(nijigenerate_node_bounds_dispatch_probe tests/node_bounds_dispatch_probe.cpp)
    target_link_libraries(nijigenerate_node_bounds_dispatch_probe PRIVATE
        nicxlive::nicxlive nijigenerate_native_options)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_node_bounds_dispatch_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_node_bounds_dispatch_probe>
                $<TARGET_FILE_DIR:nijigenerate_node_bounds_dispatch_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_PUPPET_SOURCE AND NIJIGENERATE_NICXLIVE_SOURCE_DIR)
        get_filename_component(node_bounds_original_core "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
        add_test(NAME native_node_bounds_dispatch
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/capture_node_bounds_dispatch.py"
                --native "$<TARGET_FILE:nijigenerate_node_bounds_dispatch_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/node-bounds-dispatch-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}"
                --runtime-root "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}"
                --original-node "${node_bounds_original_core}/nodes/node.d"
                --runtime-dlls "$<TARGET_RUNTIME_DLLS:nijigenerate_node_bounds_dispatch_probe>"
            COMMAND_EXPAND_LISTS)
    endif()
endif()
