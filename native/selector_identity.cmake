if(BUILD_TESTING)
    add_executable(nijigenerate_selector_identity_probe tests/selector_identity_probe.cpp)
    target_link_libraries(nijigenerate_selector_identity_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_selector_identity_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector_identity.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_selector_identity_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/selector-identity-evidence/$<CONFIG>")
    endif()
endif()
