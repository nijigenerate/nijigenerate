if(BUILD_TESTING)
    add_executable(nijigenerate_headless_tests "${CMAKE_CURRENT_LIST_DIR}/tests/headless_probe.cpp")
    target_link_libraries(nijigenerate_headless_tests PRIVATE nijigenerate_core)
    add_test(NAME native_headless_logic COMMAND nijigenerate_headless_tests)
    set_tests_properties(native_headless_logic PROPERTIES LABELS headless)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_headless_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_headless.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_headless_tests>" --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/headless-evidence/$<CONFIG>")
        set_tests_properties(native_headless_differential PROPERTIES LABELS headless)
    endif()
    add_custom_target(nijigenerate_headless_check
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PROJECT_BINARY_DIR}"
            -C "$<CONFIG>" --output-on-failure -L "^headless$"
        DEPENDS nijigenerate_headless_tests USES_TERMINAL VERBATIM)
endif()
