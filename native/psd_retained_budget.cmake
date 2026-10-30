if(BUILD_TESTING)
    add_executable(nijigenerate_psd_retained_budget_probe "${CMAKE_CURRENT_LIST_DIR}/tests/psd_retained_budget_probe.cpp")
    target_link_libraries(nijigenerate_psd_retained_budget_probe PRIVATE nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_psd_retained_budget_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_psd_retained_budget_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_psd_retained_budget.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_psd_retained_budget_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psd-retained-budget-evidence/$<CONFIG>")
    endif()
endif()
