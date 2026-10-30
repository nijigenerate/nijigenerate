if(BUILD_TESTING)
    add_executable(nijigenerate_image_enums_probe "${CMAKE_CURRENT_SOURCE_DIR}/tests/image_enums_probe.cpp")
    target_link_libraries(nijigenerate_image_enums_probe PRIVATE nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_image_enums_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_image_enum_metadata_replay
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_image_enum_metadata.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_image_enums_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/image-enum-metadata-replay/$<CONFIG>")
        add_test(NAME native_image_enums_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_image_enums.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_image_enums_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/image-enums-evidence/$<CONFIG>")
    endif()
endif()
