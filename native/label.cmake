add_library(nijigenerate_label_declarations INTERFACE)
target_include_directories(nijigenerate_label_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_label_declarations INTERFACE cxx_std_20)
target_sources(nijigenerate_label_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/label.hpp")
target_link_libraries(nijigenerate_label_declarations INTERFACE nijigenerate_imgui nijigenerate_native_options)
add_library(nijigenerate_label STATIC src/nijigenerate/widgets/label.cpp src/nijigenerate/widgets/label_link.cpp)
target_sources(nijigenerate_label PRIVATE FILE_SET label_internal_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/label_internal.hpp")
target_link_libraries(nijigenerate_label PUBLIC nijigenerate_label_declarations PRIVATE nijigenerate_dummy)
if(BUILD_TESTING)
    add_library(nijigenerate_label_header_probe OBJECT tests/label_header_probe.cpp)
    target_link_libraries(nijigenerate_label_header_probe PRIVATE nijigenerate_label_declarations)
    add_executable(nijigenerate_label_imgui_probe tests/label_imgui_probe.cpp)
    target_link_libraries(nijigenerate_label_imgui_probe PRIVATE nijigenerate_label)
    add_test(NAME native_label_imgui_contract COMMAND nijigenerate_label_imgui_probe)
    add_executable(nijigenerate_label_recording_probe tests/label_recording_probe.cpp
        src/nijigenerate/widgets/label.cpp src/nijigenerate/widgets/label_link.cpp)
    target_include_directories(nijigenerate_label_recording_probe PRIVATE tests/label_recording src)
    target_link_libraries(nijigenerate_label_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_label_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget label --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_label_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/label-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_label_project DEPENDS nijigenerate_label
        nijigenerate_label_header_probe nijigenerate_label_imgui_probe nijigenerate_label_recording_probe)
endif()
