# These actual SDL3 frame/title bodies have no Windows-only operation. Enroll
# them on every host with their real providers, rather than losing the Linux
# and macOS definitions behind the historical source filename.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_state
        AND TARGET nijigenerate_window_simple AND TARGET nijigenerate_modal
        AND TARGET nijigenerate_dialogs AND TARGET nijigenerate_label
        AND TARGET nijigenerate_dpi AND TARGET nijigenerate_gl_backend_source
        AND TARGET OpenGL::GL)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT TARGET nijigenerate_portal_filechooser)
        return()
    endif()
    get_target_property(_ng_frame_attached nijigenerate_runtime_core NG_WINDOW_FRAME_ATTACHED)
    if(_ng_frame_attached)
        return()
    endif()
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_loop_win32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_title.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_window_state
        nijigenerate_window_simple nijigenerate_modal nijigenerate_dialogs nijigenerate_label
        nijigenerate_dpi nijigenerate_gl_backend_source OpenGL::GL)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(nijigenerate_runtime_core PRIVATE nijigenerate_portal_filechooser)
    endif()
    set_property(TARGET nijigenerate_runtime_core PROPERTY NG_WINDOW_FRAME_ATTACHED TRUE)
endif()
