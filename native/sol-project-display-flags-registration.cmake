if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    add_executable(nijigenerate_project_display_flags_probe tests/project_display_flags_probe.cpp)
    target_link_libraries(nijigenerate_project_display_flags_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_project_display_flags_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_project_display_flags_probe>
                $<TARGET_FILE_DIR:nijigenerate_project_display_flags_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_project_display_flags_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_project_display_flags.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_project_display_flags_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/project-display-flags-evidence/$<CONFIG>"
                --runtime-dlls "$<TARGET_RUNTIME_DLLS:nijigenerate_project_display_flags_probe>"
            COMMAND_EXPAND_LISTS)
    endif()
endif()
