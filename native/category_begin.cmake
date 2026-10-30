if(TARGET nijigenerate_category AND TARGET nijigenerate_window_theme AND TARGET nijigenerate_dummy)
    target_sources(nijigenerate_category PRIVATE src/nijigenerate/widgets/category_begin.cpp
        src/nijigenerate/widgets/category_default.cpp)
    target_link_libraries(nijigenerate_category PRIVATE nijigenerate_window_theme nijigenerate_dummy)
    if(BUILD_TESTING)
        add_executable(nijigenerate_category_begin_imgui_probe tests/category_begin_imgui_probe.cpp)
        target_link_libraries(nijigenerate_category_begin_imgui_probe PRIVATE nijigenerate_category nijigenerate_window_theme)
        add_dependencies(nijigenerate_category_project nijigenerate_category_begin_imgui_probe)
        add_test(NAME native_category_begin_imgui COMMAND nijigenerate_category_begin_imgui_probe)
        add_executable(nijigenerate_category_begin_recording_probe tests/category_begin_recording_probe.cpp
            src/nijigenerate/widgets/category_begin.cpp src/nijigenerate/widgets/category_default.cpp
            src/nijigenerate/widgets/category_colors.cpp
            src/nijigenerate/widgets/category_readonly.cpp)
        target_include_directories(nijigenerate_category_begin_recording_probe PRIVATE tests/category_begin_recording src)
        target_link_libraries(nijigenerate_category_begin_recording_probe PRIVATE nijigenerate_native_options)
        add_dependencies(nijigenerate_category_project nijigenerate_category_begin_recording_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_INMATH_SOURCE_DIR)
            add_test(NAME native_category_begin_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_button_color.py"
                    --widget category --compiler "${NIJIGENERATE_D_COMPILER}"
                    --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --native "$<TARGET_FILE:nijigenerate_category_begin_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/category-begin-differential/$<CONFIG>")
        endif()
    endif()
endif()
