# Stage inclusion after nijigenerate_runtime_core is defined, after freeze release.
if(BUILD_TESTING)
    add_executable(nijigenerate_editor_predicate_pair_probe tests/editor_predicate_pair_probe.cpp)
    target_link_libraries(nijigenerate_editor_predicate_pair_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_editor_predicate_pair_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_editor_predicate_pair_probe>
                $<TARGET_FILE_DIR:nijigenerate_editor_predicate_pair_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_editor_predicate_pair_null_and_vec2
        COMMAND nijigenerate_editor_predicate_pair_probe)
endif()
