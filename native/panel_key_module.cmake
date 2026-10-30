if(BUILD_TESTING AND TARGET nijigenerate_core AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
    add_executable(nijigenerate_panel_key_module_probe tests/panel_key_module_probe.cpp)
    target_link_libraries(nijigenerate_panel_key_module_probe PRIVATE nijigenerate_core)
    add_test(NAME native_panel_key_module_replay
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_panel_key_module.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --native "$<TARGET_FILE:nijigenerate_panel_key_module_probe>" --configuration "$<CONFIG>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/panel-key-module/$<CONFIG>")
endif()
