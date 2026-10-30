if(BUILD_TESTING)
    add_executable(nijigenerate_image_channels_probe "${CMAKE_CURRENT_SOURCE_DIR}/tests/image_channels_probe.cpp")
    target_link_libraries(nijigenerate_image_channels_probe PRIVATE nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_image_channels_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_image_channel_table_replay
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_image_channel_table.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_image_channels_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/image-channel-table-replay/$<CONFIG>")
        add_test(NAME native_image_channels_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_image_channels.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_image_channels_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/image-channels-evidence/$<CONFIG>")
    endif()
endif()
