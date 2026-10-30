if(BUILD_TESTING)
    add_executable(nijigenerate_automesh_dynamic_disabled_log_probe tests/automesh_dynamic_disabled_log_probe.cpp)
    target_link_libraries(nijigenerate_automesh_dynamic_disabled_log_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_automesh_disabled_log_probe tests/automesh_disabled_log_probe.cpp)
    target_link_libraries(nijigenerate_automesh_disabled_log_probe PRIVATE nijigenerate_core)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_disabled_log_probe PRIVATE /Z7)
        target_compile_options(nijigenerate_automesh_dynamic_disabled_log_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_automesh_dynamic_disabled_log_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automesh_disabled_log.py"
                --module dynamic --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_automesh_dynamic_disabled_log_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-dynamic-disabled-log-evidence/$<CONFIG>")
        add_test(NAME native_automesh_disabled_log_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automesh_disabled_log.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_automesh_disabled_log_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-disabled-log-evidence/$<CONFIG>")
    endif()
endif()
