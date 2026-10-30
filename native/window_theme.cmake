if(NIJIGENERATE_IMGUI)
    include("${CMAKE_CURRENT_SOURCE_DIR}/window_declarations.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/window_background_contract.cmake")
    add_library(nijigenerate_window_theme STATIC src/nijigenerate/core/window_theme.cpp
        src/nijigenerate/core/window_theme_styling.cpp
        src/nijigenerate/platform/window_style.cpp)
    target_include_directories(nijigenerate_window_theme PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_window_theme PUBLIC nijigenerate_imgui nijigenerate_native_options
        PRIVATE nijigenerate_button)
    if(TARGET nijigenerate_window_declarations AND TARGET nijigenerate_runtime_core)
        add_library(nijigenerate_window_theme_settings STATIC src/nijigenerate/core/window_theme_settings.cpp)
        target_link_libraries(nijigenerate_window_theme_settings PUBLIC
            nijigenerate_window_declarations nijigenerate_window_theme nijigenerate_runtime_core)
    endif()
    if(BUILD_TESTING)
        add_executable(nijigenerate_window_theme_imgui_probe tests/window_theme_imgui_probe.cpp)
        target_link_libraries(nijigenerate_window_theme_imgui_probe PRIVATE nijigenerate_window_theme nijigenerate_button)
        add_executable(nijigenerate_window_theme_recording_probe tests/window_theme_recording_probe.cpp
            src/nijigenerate/core/window_theme.cpp src/nijigenerate/core/window_theme_styling.cpp
        src/nijigenerate/platform/window_style.cpp)
        target_include_directories(nijigenerate_window_theme_recording_probe PRIVATE tests/window_theme_recording src)
        target_link_libraries(nijigenerate_window_theme_recording_probe PRIVATE nijigenerate_native_options)
        add_custom_target(nijigenerate_window_theme_project DEPENDS nijigenerate_window_theme
            nijigenerate_window_theme_imgui_probe nijigenerate_window_theme_recording_probe)
        add_test(NAME native_window_theme_imgui_contract COMMAND nijigenerate_window_theme_imgui_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_IMGUI_BINDING_SOURCE)
            add_test(NAME native_window_theme_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_window_theme.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --binding "${NIJIGENERATE_IMGUI_BINDING_SOURCE}"
                    --native "$<TARGET_FILE:nijigenerate_window_theme_recording_probe>"
                    --real-native "$<TARGET_FILE:nijigenerate_window_theme_imgui_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/window-theme-differential/$<CONFIG>")
        endif()
    endif()
endif()
