if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base
        AND TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_io)
    set(_settings_window_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/settings.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/settings_shortcuts.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_settings_window_sources})
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET settings_window_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/settings.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/settings_templates.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_window_base
        nijigenerate_io nijigenerate_autosave nijigenerate_gl_backend_source
        nijigenerate_label nijigenerate_button nijigenerate_dummy nijigenerate_tooltip)
    if(MSVC)
        set_property(SOURCE ${_settings_window_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_settings_window_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_settings_window_sources)
endif()
