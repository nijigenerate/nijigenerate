# Pending next whole-project batch; not included during the active build.
if(BUILD_TESTING)
    add_executable(nijigenerate_psd_mask_bpp_probe tests/psd_mask_bpp_probe.cpp)
    target_link_libraries(nijigenerate_psd_mask_bpp_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_psd_mask_bpp_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_psd_mask_bpp.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_psd_mask_bpp_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psd-mask-bpp-evidence/$<CONFIG>")
    endif()
endif()
