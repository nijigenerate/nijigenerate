if(BUILD_TESTING AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON
        AND EXISTS "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}")
    set(editor_font_runtime_args)
    if(TARGET Boost::json)
        list(APPEND editor_font_runtime_args
            --native-scope-json "$<TARGET_FILE:nijigenerate_command_scope_record_probe>")
    endif()
    # The existing probe is registered later; TARGET_FILE resolves at generation.
    if(TARGET nicxlive::nicxlive AND TARGET nijigenerate_imgui_sdl3)
        list(APPEND editor_font_runtime_args
            --native-font-state "$<TARGET_FILE:nijigenerate_automesh_config_key_probe>"
            --native-context-state "$<TARGET_FILE:nijigenerate_automesh_config_key_probe>")
    endif()
    add_test(NAME native_editor_type_contract_group
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/check_editor_type_contracts.py"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --dependency-spec "${NIJIGENERATE_D_DECLARATION_DEPENDENCY_SPEC}"
            --configuration "$<CONFIG>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/editor-type-contracts/$<CONFIG>"
            ${editor_font_runtime_args})
    set_tests_properties(native_editor_type_contract_group PROPERTIES TIMEOUT 360)
endif()
