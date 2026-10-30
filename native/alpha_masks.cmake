target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_alpha_mask.cpp)
target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_override_builder.cpp)
target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_psd_export.cpp)
target_sources(nijigenerate_core PRIVATE src/nijigenerate/io/depthimage_global_scale.cpp)
if(BUILD_TESTING)
    add_executable(nijigenerate_alpha_mask_probe tests/alpha_mask_probe.cpp)
    target_link_libraries(nijigenerate_alpha_mask_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depth_override_builder_probe tests/depth_override_builder_probe.cpp)
    target_link_libraries(nijigenerate_depth_override_builder_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_psd_export_probe tests/psd_export_probe.cpp)
    target_link_libraries(nijigenerate_psd_export_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depth_global_scale_probe tests/depth_global_scale_probe.cpp)
    target_link_libraries(nijigenerate_depth_global_scale_probe PRIVATE nijigenerate_core)
    add_custom_target(nijigenerate_alpha_masks_project DEPENDS nijigenerate_core nijigenerate_alpha_mask_probe
        nijigenerate_depth_override_builder_probe nijigenerate_psd_export_probe nijigenerate_depth_global_scale_probe)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depth_global_scale_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_global_scale.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_global_scale_probe>"
                --library "$<TARGET_FILE:nijigenerate_core>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/global-scale-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
        add_test(NAME native_alpha_masks_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_alpha_masks.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_alpha_mask_probe>"
                --library "$<TARGET_FILE:nijigenerate_core>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/alpha-mask-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
        add_test(NAME native_depth_override_builder_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_override_builder.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_override_builder_probe>"
                --library "$<TARGET_FILE:nijigenerate_core>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/override-builder-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
        add_test(NAME native_psd_export_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_psd_export.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_psd_export_probe>"
                --library "$<TARGET_FILE:nijigenerate_core>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psd-export-evidence/$<CONFIG>"
                --configuration "$<CONFIG>")
    endif()
endif()
