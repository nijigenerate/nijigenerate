if(BUILD_TESTING AND TARGET nijigenerate_runtime_core AND
    NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_node_base_module_probe tests/node_base_module_probe.cpp)
    target_link_libraries(nijigenerate_node_base_module_probe PRIVATE nijigenerate_runtime_core Threads::Threads)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_node_base_module_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_node_base_module_probe>
                $<TARGET_FILE_DIR:nijigenerate_node_base_module_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_node_base_module_replay
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_node_base_module.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
            --native "$<TARGET_FILE:nijigenerate_node_base_module_probe>"
            --configuration "$<CONFIG>" --output "${CMAKE_CURRENT_BINARY_DIR}/node-base-module/$<CONFIG>")
endif()
