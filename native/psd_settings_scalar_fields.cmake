if(BUILD_TESTING)
    add_executable(nijigenerate_psd_settings_scalar_fields_probe tests/psd_settings_scalar_fields_probe.cpp)
    target_link_libraries(nijigenerate_psd_settings_scalar_fields_probe PRIVATE nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_psd_settings_scalar_fields_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_psd_settings_scalar_fields_replay
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_psd_settings_scalar_fields.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_psd_settings_scalar_fields_probe>" --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psd-settings-scalar-fields-replay/$<CONFIG>")
    endif()
endif()
