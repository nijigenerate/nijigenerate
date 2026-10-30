get_filename_component(_mutation_editor_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
target_sources(nijigenerate_core PRIVATE
    "${_mutation_editor_root}/native/src/nijigenerate/actions/depthbone_publishers.cpp")
if(BUILD_TESTING AND NIJIGENERATE_RUNTIME_PROBE AND NOT TARGET nijigenerate_mutation_publishers_probe)
    add_executable(nijigenerate_mutation_publishers_probe
        "${_mutation_editor_root}/native/tests/mutation_publishers_probe.cpp")
    target_link_libraries(nijigenerate_mutation_publishers_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_mutation_publishers_probe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_mutation_publishers_probe>
                $<TARGET_FILE_DIR:nijigenerate_mutation_publishers_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_mutation_publishers_file COMMAND nijigenerate_mutation_publishers_probe)
    add_test(NAME native_mutation_publishers_assertion COMMAND nijigenerate_mutation_publishers_probe --invalid)
    set_tests_properties(native_mutation_publishers_file native_mutation_publishers_assertion
        PROPERTIES TIMEOUT 30)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON
            AND EXISTS "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}")
        set(_publisher_reference "${CMAKE_CURRENT_BINARY_DIR}/mutation-publishers-reference/$<CONFIG>")
        add_test(NAME native_mutation_publishers_original_group
            COMMAND "${NIJIGENERATE_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mutation_publishers_runtime.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
                --configuration "$<CONFIG>" --output "${_publisher_reference}")
        set_tests_properties(native_mutation_publishers_original_group PROPERTIES
            FIXTURES_SETUP mutation_publishers_original TIMEOUT 180)
        add_test(NAME native_mutation_publishers_differential
            COMMAND "${NIJIGENERATE_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_mutation_publishers.py"
                --d-evidence "${_publisher_reference}/latest.json"
                --native "$<TARGET_FILE:nijigenerate_mutation_publishers_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/mutation-publishers-comparison/$<CONFIG>")
        set_tests_properties(native_mutation_publishers_differential PROPERTIES
            FIXTURES_REQUIRED mutation_publishers_original TIMEOUT 150)
    endif()
endif()
