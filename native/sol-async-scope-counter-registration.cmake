if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    add_executable(nijigenerate_async_scope_counter_probe tests/async_scope_counter_probe.cpp)
    target_link_libraries(nijigenerate_async_scope_counter_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_async_scope_counter_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_async_scope_counter_probe>
                $<TARGET_FILE_DIR:nijigenerate_async_scope_counter_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_async_scope_counter_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_async_scope_counter.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_async_scope_counter_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/async-scope-counter-evidence/$<CONFIG>")
    endif()
endif()
