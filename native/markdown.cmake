add_library(nijigenerate_markdown_declarations INTERFACE)
target_include_directories(nijigenerate_markdown_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_markdown_declarations INTERFACE cxx_std_20)
target_link_libraries(nijigenerate_markdown_declarations INTERFACE nijigenerate_native_options nijigenerate_imgui)
target_sources(nijigenerate_markdown_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/markdown.hpp")
add_library(nijigenerate_markdown STATIC src/nijigenerate/widgets/markdown_underline.cpp
    src/nijigenerate/widgets/markdown_format.cpp src/nijigenerate/widgets/markdown_renderer.cpp)
target_sources(nijigenerate_markdown PRIVATE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/markdown_internal.hpp")
target_link_libraries(nijigenerate_markdown PUBLIC nijigenerate_markdown_declarations nijigenerate_imgui nijigenerate_dummy)
if(BUILD_TESTING)
    add_library(nijigenerate_markdown_header_probe OBJECT tests/markdown_header_probe.cpp)
    target_link_libraries(nijigenerate_markdown_header_probe PRIVATE nijigenerate_markdown_declarations)
    add_executable(nijigenerate_markdown_link_data_probe tests/markdown_link_data_probe.cpp)
    target_link_libraries(nijigenerate_markdown_link_data_probe PRIVATE nijigenerate_markdown_declarations)
    add_executable(nijigenerate_markdown_defaults_probe tests/markdown_defaults_probe.cpp)
    target_link_libraries(nijigenerate_markdown_defaults_probe PRIVATE nijigenerate_markdown)
    add_executable(nijigenerate_markdown_underline_recording_probe
        tests/markdown_underline_recording_probe.cpp src/nijigenerate/widgets/markdown_underline.cpp)
    target_include_directories(nijigenerate_markdown_underline_recording_probe PRIVATE tests/markdown_recording src)
    target_link_libraries(nijigenerate_markdown_underline_recording_probe PRIVATE nijigenerate_native_options)
    add_executable(nijigenerate_markdown_underline_imgui_probe tests/markdown_underline_imgui_probe.cpp)
    target_link_libraries(nijigenerate_markdown_underline_imgui_probe PRIVATE nijigenerate_markdown)
    add_test(NAME native_markdown_underline_imgui_contract COMMAND nijigenerate_markdown_underline_imgui_probe)
    add_executable(nijigenerate_markdown_format_recording_probe
        tests/markdown_format_recording_probe.cpp src/nijigenerate/widgets/markdown_format.cpp)
    target_include_directories(nijigenerate_markdown_format_recording_probe PRIVATE tests/markdown_recording src)
    target_link_libraries(nijigenerate_markdown_format_recording_probe PRIVATE nijigenerate_native_options)
    add_executable(nijigenerate_markdown_format_imgui_probe tests/markdown_format_imgui_probe.cpp)
    target_link_libraries(nijigenerate_markdown_format_imgui_probe PRIVATE nijigenerate_markdown)
    add_test(NAME native_markdown_format_imgui_contract COMMAND nijigenerate_markdown_format_imgui_probe)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_D_IMGUI_SOURCE_DIR)
        add_test(NAME native_markdown_format_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_markdown_format.py"
                --native "$<TARGET_FILE:nijigenerate_markdown_format_recording_probe>"
                --reference "${CMAKE_CURRENT_BINARY_DIR}/markdown-types-original/$<CONFIG>/report.json"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-format-differential/$<CONFIG>")
        add_test(NAME native_markdown_underline_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_markdown_underline.py"
                --native "$<TARGET_FILE:nijigenerate_markdown_underline_recording_probe>"
                --reference "${CMAKE_CURRENT_BINARY_DIR}/markdown-underline-original/$<CONFIG>/report.json"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-underline-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_markdown_project DEPENDS nijigenerate_markdown_header_probe
        nijigenerate_markdown nijigenerate_markdown_underline_recording_probe nijigenerate_markdown_underline_imgui_probe
        nijigenerate_markdown_format_recording_probe nijigenerate_markdown_format_imgui_probe
        nijigenerate_markdown_link_data_probe nijigenerate_markdown_defaults_probe)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_D_IMGUI_SOURCE_DIR)
        add_test(NAME native_markdown_types_original_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/markdown_types_contract.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-types-original/$<CONFIG>")
        set_tests_properties(native_markdown_types_original_contract PROPERTIES
            FIXTURES_SETUP markdown_types_reference)
        set_tests_properties(native_markdown_format_differential PROPERTIES
            FIXTURES_REQUIRED markdown_types_reference)
        add_test(NAME native_markdown_defaults_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_markdown_defaults.py"
                --native "$<TARGET_FILE:nijigenerate_markdown_defaults_probe>"
                --reference "${CMAKE_CURRENT_BINARY_DIR}/markdown-types-original/$<CONFIG>/report.json"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-defaults-differential/$<CONFIG>")
        set_tests_properties(native_markdown_defaults_differential PROPERTIES FIXTURES_REQUIRED markdown_types_reference)
        add_test(NAME native_markdown_underline_original_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/markdown_underline_contract.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-underline-original/$<CONFIG>")
        set_tests_properties(native_markdown_underline_original_contract PROPERTIES
            FIXTURES_SETUP markdown_underline_reference)
        set_tests_properties(native_markdown_underline_differential PROPERTIES
            FIXTURES_REQUIRED markdown_underline_reference)
        add_test(NAME native_markdown_data_original_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/markdown_data_contract.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-data-original/$<CONFIG>")
        set_tests_properties(native_markdown_data_original_contract PROPERTIES FIXTURES_SETUP markdown_data_reference)
        add_test(NAME native_markdown_link_data_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_markdown_link_data.py"
                --native "$<TARGET_FILE:nijigenerate_markdown_link_data_probe>"
                --reference "${CMAKE_CURRENT_BINARY_DIR}/markdown-data-original/$<CONFIG>/report.json"
                --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-link-data-differential/$<CONFIG>")
        set_tests_properties(native_markdown_link_data_differential PROPERTIES FIXTURES_REQUIRED markdown_data_reference)
    endif()
endif()
