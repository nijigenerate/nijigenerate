if(BUILD_TESTING)
    add_executable(nijigenerate_automesh_config_key_probe tests/automesh_config_key_probe.cpp)
    target_link_libraries(nijigenerate_automesh_config_key_probe PRIVATE nijigenerate_core)
    target_sources(nijigenerate_automesh_config_key_probe PRIVATE tests/dynamic_node_key_types.cpp)
    set(config_key_selection_args)
    if(TARGET nicxlive::nicxlive AND TARGET nijigenerate_imgui_sdl3)
        find_package(Threads REQUIRED)
        target_link_libraries(nijigenerate_automesh_config_key_probe PRIVATE
            nicxlive::nicxlive nijigenerate_imgui_sdl3 Threads::Threads)
        target_sources(nijigenerate_automesh_config_key_probe PRIVATE
            tests/font_init_state_observation.cpp tests/context_state_observation.cpp)
        target_compile_definitions(nijigenerate_automesh_config_key_probe PRIVATE
            NIJIGENERATE_SELECT_STATE_RUNTIME=1 NIJIGENERATE_FONT_STATE_RUNTIME=1
            NIJIGENERATE_CONTEXT_RUNTIME=1)
        list(APPEND config_key_selection_args --selection-runtime)
        if(WIN32)
            add_custom_command(TARGET nijigenerate_automesh_config_key_probe POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:nijigenerate_automesh_config_key_probe>
                    $<TARGET_FILE_DIR:nijigenerate_automesh_config_key_probe>
                COMMAND_EXPAND_LISTS)
        endif()
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        get_filename_component(config_key_d_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        add_test(NAME native_automesh_config_key_group
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/automesh_config_key_compare.py"
                --source-root "${CMAKE_CURRENT_SOURCE_DIR}/.."
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${config_key_d_bin}/../import"
                --native "$<TARGET_FILE:nijigenerate_automesh_config_key_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-config-key-evidence/$<CONFIG>"
                ${config_key_selection_args})
        set_tests_properties(native_automesh_config_key_group PROPERTIES TIMEOUT 120)
    endif()
endif()
