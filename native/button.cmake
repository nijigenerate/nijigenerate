add_library(nijigenerate_button_declarations INTERFACE)
target_include_directories(nijigenerate_button_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_button_declarations INTERFACE cxx_std_20)
target_link_libraries(nijigenerate_button_declarations INTERFACE nijigenerate_imgui nijigenerate_native_options)
add_library(nijigenerate_button STATIC src/nijigenerate/widgets/button.cpp src/nijigenerate/widgets/button_tabs.cpp
    src/nijigenerate/widgets/button_dropdown.cpp src/nijigenerate/widgets/button_menu_begin.cpp
    src/nijigenerate/widgets/button_menu_end.cpp
    src/nijigenerate/widgets/button_checkbox.cpp)
target_link_libraries(nijigenerate_button PUBLIC nijigenerate_button_declarations)
target_sources(nijigenerate_button PUBLIC
    FILE_SET button_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/button.hpp")
target_link_libraries(nijigenerate_button PRIVATE nijigenerate_dummy)
target_link_libraries(nijigenerate_button PRIVATE nijigenerate_toggle)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_button_color_probe tests/button_color_probe.cpp)
    target_link_libraries(nijigenerate_button_color_probe PRIVATE nijigenerate_button Threads::Threads)
    add_test(NAME native_button_color_contract COMMAND nijigenerate_button_color_probe)
    add_executable(nijigenerate_button_imgui_probe tests/button_imgui_probe.cpp)
    target_link_libraries(nijigenerate_button_imgui_probe PRIVATE nijigenerate_button)
    add_test(NAME native_button_imgui_contract COMMAND nijigenerate_button_imgui_probe)
    add_executable(nijigenerate_button_checkbox_imgui_probe tests/button_checkbox_imgui_probe.cpp)
    target_link_libraries(nijigenerate_button_checkbox_imgui_probe PRIVATE nijigenerate_button)
    add_test(NAME native_button_checkbox_imgui_contract COMMAND nijigenerate_button_checkbox_imgui_probe)
    add_executable(nijigenerate_button_recording_probe
        tests/button_recording_probe.cpp src/nijigenerate/widgets/button.cpp)
    target_include_directories(nijigenerate_button_recording_probe PRIVATE tests/button_recording src)
    target_link_libraries(nijigenerate_button_recording_probe PRIVATE nijigenerate_native_options Threads::Threads)
    add_executable(nijigenerate_button_tabs_recording_probe tests/button_tabs_recording_probe.cpp
        src/nijigenerate/widgets/button.cpp src/nijigenerate/widgets/button_tabs.cpp)
    target_include_directories(nijigenerate_button_tabs_recording_probe PRIVATE tests/button_recording src)
    target_link_libraries(nijigenerate_button_tabs_recording_probe PRIVATE nijigenerate_native_options)
    add_executable(nijigenerate_button_menu_recording_probe tests/button_menu_recording_probe.cpp
        src/nijigenerate/widgets/button_menu_end.cpp)
    target_include_directories(nijigenerate_button_menu_recording_probe PRIVATE tests/button_recording src)
    target_link_libraries(nijigenerate_button_menu_recording_probe PRIVATE nijigenerate_native_options)
    add_executable(nijigenerate_button_dropdown_recording_probe tests/button_dropdown_recording_probe.cpp
        src/nijigenerate/widgets/button.cpp src/nijigenerate/widgets/button_dropdown.cpp
        src/nijigenerate/widgets/dummy_layout.cpp)
    target_include_directories(nijigenerate_button_dropdown_recording_probe PRIVATE tests/dropdown_recording src)
    target_link_libraries(nijigenerate_button_dropdown_recording_probe PRIVATE nijigenerate_native_options)
    add_executable(nijigenerate_button_checkbox_recording_probe tests/button_checkbox_recording_probe.cpp
        src/nijigenerate/widgets/button_checkbox.cpp)
    target_include_directories(nijigenerate_button_checkbox_recording_probe PRIVATE tests/checkbox_recording src)
    target_link_libraries(nijigenerate_button_checkbox_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_button_color_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_button_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/button-color-differential/$<CONFIG>")
        add_test(NAME native_button_tabs_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget button_tabs --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_button_tabs_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/button-tabs-differential/$<CONFIG>")
        add_test(NAME native_button_menu_data_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget button_menu_data --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_button_menu_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/button-menu-data-differential/$<CONFIG>")
        add_test(NAME native_button_dropdown_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget button_dropdown --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_button_dropdown_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/button-dropdown-differential/$<CONFIG>")
        add_test(NAME native_button_checkbox_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget button_checkbox --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_button_checkbox_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/button-checkbox-differential/$<CONFIG>")
    endif()
    add_library(nijigenerate_button_header_probe OBJECT tests/button_header_probe.cpp)
    target_link_libraries(nijigenerate_button_header_probe PRIVATE nijigenerate_button_declarations)
    add_custom_target(nijigenerate_button_project DEPENDS nijigenerate_button
        nijigenerate_button_header_probe nijigenerate_button_color_probe
        nijigenerate_button_imgui_probe nijigenerate_button_recording_probe nijigenerate_button_tabs_recording_probe
        nijigenerate_button_menu_recording_probe nijigenerate_button_dropdown_recording_probe
        nijigenerate_button_checkbox_imgui_probe nijigenerate_button_checkbox_recording_probe)
endif()
