if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    add_executable(nijigenerate_project_lifetime_probe tests/project_lifetime_probe.cpp)
    target_link_libraries(nijigenerate_project_lifetime_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_project_lifetime_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_project_lifetime_probe>
                $<TARGET_FILE_DIR:nijigenerate_project_lifetime_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_PYTHON)
        add_test(NAME native_project_lifetime_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_project_lifetime.py"
                --native "$<TARGET_FILE:nijigenerate_project_lifetime_probe>" --config "$<CONFIG>"
                --compiler "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/project-lifetime-evidence/$<CONFIG>/result.json")
    else()
        add_test(NAME native_project_lifetime_contract COMMAND nijigenerate_project_lifetime_probe)
    endif()
endif()
