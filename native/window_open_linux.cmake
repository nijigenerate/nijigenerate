if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET nijigenerate_window_context
        AND TARGET nijigenerate_window_factory AND TARGET nijigenerate_logo
        AND TARGET nijigenerate_resources AND TARGET nijigenerate_portal_filechooser
        AND TARGET nijigenerate_window_platform AND TARGET nijigenerate_window_debug
        AND NOT TARGET nijigenerate_window_open_linux)
    add_library(nijigenerate_window_open_linux STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/window_open_linux.cpp")
    target_link_libraries(nijigenerate_window_open_linux PUBLIC nijigenerate_window_declarations
        PRIVATE nijigenerate_window_factory nijigenerate_window_context
            nijigenerate_window_sdl3_binding nijigenerate_runtime_core nijigenerate_gl_backend_source
            nijigenerate_logo nijigenerate_resources nijigenerate_portal_filechooser
            nijigenerate_window_platform nijigenerate_core nijigenerate_window_debug)
    target_compile_features(nijigenerate_window_open_linux PUBLIC cxx_std_20)
    target_compile_definitions(nijigenerate_window_open_linux PRIVATE
        $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
endif()
