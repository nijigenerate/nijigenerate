if(BUILD_TESTING)
    add_executable(nijigenerate_command_scope_record_probe tests/command_scope_record_probe.cpp)
    target_link_libraries(nijigenerate_command_scope_record_probe PRIVATE nijigenerate_core)
    if(TARGET Boost::json)
        target_sources(nijigenerate_command_scope_record_probe PRIVATE tests/command_scope_json_observation.cpp)
        target_compile_definitions(nijigenerate_command_scope_record_probe PRIVATE NIJIGENERATE_SCOPE_JSON_OBSERVATION=1)
        target_link_libraries(nijigenerate_command_scope_record_probe PRIVATE Boost::json)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        get_filename_component(scope_record_d_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        add_test(NAME native_command_scope_record_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/command_scope_record_compare.py"
                --source-root "${CMAKE_CURRENT_SOURCE_DIR}/.."
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${scope_record_d_bin}/../import"
                --native "$<TARGET_FILE:nijigenerate_command_scope_record_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/command-scope-record-evidence/$<CONFIG>")
    endif()
endif()
