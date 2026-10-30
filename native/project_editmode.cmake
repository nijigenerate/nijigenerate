if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    add_executable(nijigenerate_project_editmode_probe tests/project_editmode_probe.cpp)
    target_link_libraries(nijigenerate_project_editmode_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_project_editmode_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_project_editmode_probe>
                $<TARGET_FILE_DIR:nijigenerate_project_editmode_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_project_editmode_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_project_editmode.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_project_editmode_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/project-editmode-evidence/$<CONFIG>")
    endif()
endif()
