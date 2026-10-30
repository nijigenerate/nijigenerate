# Include after the actual window frame, save, task and GL providers are selected.
function(ng_attach_window_event_pump)
    foreach(provider IN ITEMS nijigenerate_runtime_core nijigenerate_window_state
            nijigenerate_tasks nijigenerate_io_save nijigenerate_gl_backend_source
            nijigenerate_window_simple nijigenerate_modal nijigenerate_dialogs
            nijigenerate_label nijigenerate_dpi OpenGL::GL)
        if(NOT TARGET "${provider}")
            message(FATAL_ERROR "Window event pump requires actual provider ${provider}")
        endif()
    endforeach()
    # ngBeginLoop calls the actual ngBeginLoopNoEv frame body. Its current
    # producer selects this source on Windows; another platform must select a
    # genuine frame producer before attaching the pump, rather than omitting it.
    get_target_property(_ng_pump_sources nijigenerate_runtime_core SOURCES)
    list(FIND _ng_pump_sources
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/core/window_loop_win32.cpp"
        _ng_pump_frame_index)
    if(_ng_pump_frame_index EQUAL -1)
        message(FATAL_ERROR "Window event pump requires the actual selected ngBeginLoopNoEv frame source")
    endif()
    get_target_property(attached nijigenerate_runtime_core NG_WINDOW_EVENT_PUMP_ATTACHED)
    if(NOT attached)
        target_sources(nijigenerate_runtime_core PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/core/window_event_pump.cpp")
        target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_window_state
            nijigenerate_tasks nijigenerate_io_save nijigenerate_gl_backend_source)
        target_compile_features(nijigenerate_runtime_core PUBLIC cxx_std_20)
        set_property(TARGET nijigenerate_runtime_core PROPERTY NG_WINDOW_EVENT_PUMP_ATTACHED TRUE)
    endif()
endfunction()
