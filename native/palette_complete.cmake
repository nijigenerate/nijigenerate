if(TARGET nijigenerate_palette_state AND TARGET nijigenerate_runtime_core)
    target_link_libraries(nijigenerate_palette_state PUBLIC nijigenerate_runtime_core
        nijigenerate_notification nijigenerate_inputtext nijigenerate_dummy)
endif()
if(TARGET nijigenerate_command_scope_json AND TARGET nijigenerate_runtime_core)
    target_link_libraries(nijigenerate_command_scope_json PUBLIC nijigenerate_runtime_core)
endif()
