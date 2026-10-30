if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_clipboard_parameter_state_probe tests/clipboard_parameter_state_probe.cpp)
    target_link_libraries(nijigenerate_clipboard_parameter_state_probe PRIVATE nijigenerate_runtime_core Threads::Threads)
    if(MSVC)
        target_compile_options(nijigenerate_clipboard_parameter_state_probe PRIVATE /Z7)
    endif()
    if(WIN32)
        add_custom_command(TARGET nijigenerate_clipboard_parameter_state_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_clipboard_parameter_state_probe>
                $<TARGET_FILE_DIR:nijigenerate_clipboard_parameter_state_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_clipboard_parameter_state_null COMMAND nijigenerate_clipboard_parameter_state_probe)
endif()
