if(TARGET nijigenerate_imgui AND TARGET nijigenerate_window_platform)
    include("${CMAKE_CURRENT_SOURCE_DIR}/widgets_public.cmake")
    add_library(nijigenerate_widget_window STATIC src/nijigenerate/widgets/window_end.cpp
        src/nijigenerate/widgets/window_begin.cpp)
    target_include_directories(nijigenerate_widget_window PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_widget_window PUBLIC nijigenerate_imgui nijigenerate_native_options)
    target_compile_definitions(nijigenerate_widget_window PRIVATE NG_USE_UI_SCALING)
    add_library(nijigenerate_widget_window_no_scaling STATIC src/nijigenerate/widgets/window_end.cpp
        src/nijigenerate/widgets/window_begin.cpp)
    target_include_directories(nijigenerate_widget_window_no_scaling PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(nijigenerate_widget_window_no_scaling PRIVATE NG_NO_UI_SCALING)
    target_link_libraries(nijigenerate_widget_window_no_scaling PUBLIC nijigenerate_imgui nijigenerate_native_options
        PRIVATE nijigenerate_window_platform)
    foreach(_ng_widget_window IN ITEMS nijigenerate_widget_window nijigenerate_widget_window_no_scaling)
        target_compile_features(${_ng_widget_window} PUBLIC cxx_std_20)
        target_link_libraries(${_ng_widget_window} PUBLIC nijigenerate_widgets_declarations)
        target_sources(${_ng_widget_window} PUBLIC FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/window.hpp")
    endforeach()
    unset(_ng_widget_window)
    if(BUILD_TESTING)
        add_library(nijigenerate_widget_window_header_probe OBJECT tests/widget_window_header_probe.cpp)
        target_link_libraries(nijigenerate_widget_window_header_probe PRIVATE nijigenerate_widget_window)
        add_executable(nijigenerate_widget_end_imgui_probe tests/widget_end_imgui_probe.cpp)
        target_link_libraries(nijigenerate_widget_end_imgui_probe PRIVATE nijigenerate_widget_window)
        add_executable(nijigenerate_widget_end_recording_probe tests/widget_end_recording_probe.cpp
            src/nijigenerate/widgets/window_end.cpp)
        target_include_directories(nijigenerate_widget_end_recording_probe PRIVATE tests/widget_window_recording src)
        target_link_libraries(nijigenerate_widget_end_recording_probe PRIVATE nijigenerate_native_options)
        add_custom_target(nijigenerate_widget_window_project DEPENDS nijigenerate_widget_window
            nijigenerate_widget_window_header_probe nijigenerate_widget_end_imgui_probe
            nijigenerate_widget_end_recording_probe)
        foreach(window_mode IN ITEMS scaling no_scaling)
            if(window_mode STREQUAL "scaling")
                set(window_library nijigenerate_widget_window)
                set(window_definition NG_USE_UI_SCALING)
            else()
                set(window_library nijigenerate_widget_window_no_scaling)
                set(window_definition NG_NO_UI_SCALING)
            endif()
            add_executable(nijigenerate_widget_begin_${window_mode}_imgui_probe tests/widget_begin_imgui_probe.cpp)
            target_compile_definitions(nijigenerate_widget_begin_${window_mode}_imgui_probe PRIVATE ${window_definition})
            target_link_libraries(nijigenerate_widget_begin_${window_mode}_imgui_probe PRIVATE
                ${window_library} nijigenerate_window_platform)
            add_executable(nijigenerate_widget_begin_${window_mode}_recording_probe tests/widget_begin_recording_probe.cpp
                src/nijigenerate/widgets/window_begin.cpp)
            target_compile_definitions(nijigenerate_widget_begin_${window_mode}_recording_probe PRIVATE ${window_definition})
            target_include_directories(nijigenerate_widget_begin_${window_mode}_recording_probe PRIVATE tests/widget_window_recording src)
            target_link_libraries(nijigenerate_widget_begin_${window_mode}_recording_probe PRIVATE nijigenerate_native_options)
            add_dependencies(nijigenerate_widget_window_project
                nijigenerate_widget_begin_${window_mode}_imgui_probe nijigenerate_widget_begin_${window_mode}_recording_probe)
            add_test(NAME native_widget_begin_${window_mode}_imgui COMMAND nijigenerate_widget_begin_${window_mode}_imgui_probe)
        endforeach()
        add_test(NAME native_widget_end_imgui COMMAND nijigenerate_widget_end_imgui_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
            add_test(NAME native_widget_end_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_widget_end.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_widget_end_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/widget-end-differential/$<CONFIG>")
            if(NIJIGENERATE_D_IMGUI_SOURCE_DIR)
                add_test(NAME native_widget_begin_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_widget_begin.py"
                        --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                        --native-use "$<TARGET_FILE:nijigenerate_widget_begin_scaling_recording_probe>"
                        --native-no "$<TARGET_FILE:nijigenerate_widget_begin_no_scaling_recording_probe>"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/widget-begin-differential/$<CONFIG>")
            endif()
        endif()
    endif()
endif()
