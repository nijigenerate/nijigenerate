# Actual window/context bodies are product sources, not probe-only features.
# Missing real providers leave this source boundary unavailable; no substitute
# context, window factory, renderer, fonts or dialogs are supplied.
set(ng_window_context_providers_ready TRUE)
foreach(ng_window_context_provider IN ITEMS nijigenerate_window_declarations
        nijigenerate_runtime_core nijigenerate_core_path nijigenerate_window_theme_settings
        nijigenerate_window_simple nijigenerate_dpi nijigenerate_font
        nijigenerate_dialogs nijigenerate_window_background
        nijigenerate_gl_backend_source nijigenerate_imgui_sdl3)
    if(NOT TARGET "${ng_window_context_provider}")
        set(ng_window_context_providers_ready FALSE)
    endif()
endforeach()
if(ng_window_context_providers_ready AND NOT TARGET nijigenerate_window_context)
    add_library(nijigenerate_window_context STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_context.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/window_context_policy.cpp")
    target_compile_features(nijigenerate_window_context PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_window_context PUBLIC nijigenerate_window_declarations
        PRIVATE nijigenerate_runtime_core nijigenerate_core_path nijigenerate_window_theme_settings
            nijigenerate_window_simple nijigenerate_dpi nijigenerate_font
            nijigenerate_dialogs nijigenerate_window_background
            nijigenerate_gl_backend_source nijigenerate_imgui_sdl3)
    if(APPLE)
        target_compile_definitions(nijigenerate_window_context PRIVATE NG_OSX)
    endif()
endif()
unset(ng_window_context_provider)
unset(ng_window_context_providers_ready)

if(TARGET nijigenerate_window_context AND TARGET nijigenerate_window_factory
        AND TARGET nijigenerate_logo AND TARGET nijigenerate_window_sdl3_binding
        AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_gl_backend_source)
    if(WIN32 AND TARGET nijigenerate_window_win32_dpi
            AND NOT TARGET nijigenerate_window_open_win32)
        add_library(nijigenerate_window_open_win32 STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/window_open_win32.cpp")
        target_compile_features(nijigenerate_window_open_win32 PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_window_open_win32 PUBLIC nijigenerate_window_declarations
            PRIVATE nijigenerate_window_factory nijigenerate_window_context
                nijigenerate_window_win32_dpi nijigenerate_window_sdl3_binding
                nijigenerate_runtime_core nijigenerate_gl_backend_source nijigenerate_logo)
        target_compile_definitions(nijigenerate_window_open_win32 PRIVATE
            $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
    endif()
    if(APPLE AND TARGET nijigenerate_core AND TARGET nijigenerate_window_debug
            AND NOT TARGET nijigenerate_window_open_macos)
        add_library(nijigenerate_window_open_macos STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/window_open_macos.cpp")
        target_compile_features(nijigenerate_window_open_macos PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_window_open_macos PUBLIC nijigenerate_window_declarations
            PRIVATE nijigenerate_window_factory nijigenerate_window_context
                nijigenerate_window_sdl3_binding nijigenerate_runtime_core
                nijigenerate_gl_backend_source nijigenerate_logo nijigenerate_core nijigenerate_window_debug)
        target_compile_definitions(nijigenerate_window_open_macos PRIVATE
            $<$<CONFIG:Debug>:NIJIGENERATE_WINDOW_ORDINARY_DEBUG=1>)
    endif()
endif()
