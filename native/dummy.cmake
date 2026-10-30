add_library(nijigenerate_dummy_declarations INTERFACE)
target_include_directories(nijigenerate_dummy_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_dummy_declarations INTERFACE cxx_std_20)
target_sources(nijigenerate_dummy_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/dummy.hpp")
target_link_libraries(nijigenerate_dummy_declarations INTERFACE nijigenerate_imgui nijigenerate_native_options)
add_library(nijigenerate_dummy STATIC src/nijigenerate/widgets/dummy_layout.cpp src/nijigenerate/widgets/dummy.cpp)
target_link_libraries(nijigenerate_dummy PUBLIC nijigenerate_dummy_declarations)
if(BUILD_TESTING)
    add_library(nijigenerate_dummy_header_probe OBJECT tests/dummy_header_probe.cpp)
    target_link_libraries(nijigenerate_dummy_header_probe PRIVATE nijigenerate_dummy_declarations)
    add_executable(nijigenerate_dummy_layout_probe tests/dummy_layout_probe.cpp)
    target_link_libraries(nijigenerate_dummy_layout_probe PRIVATE nijigenerate_dummy)
    add_test(NAME native_dummy_layout_contract COMMAND nijigenerate_dummy_layout_probe)
    add_executable(nijigenerate_dummy_recording_probe tests/dummy_recording_probe.cpp
        src/nijigenerate/widgets/dummy.cpp src/nijigenerate/widgets/dummy_layout.cpp)
    target_include_directories(nijigenerate_dummy_recording_probe PRIVATE tests/dummy_recording src)
    target_link_libraries(nijigenerate_dummy_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_dummy_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget dummy --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_dummy_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/dummy-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_dummy_project DEPENDS nijigenerate_dummy
        nijigenerate_dummy_header_probe nijigenerate_dummy_layout_probe nijigenerate_dummy_recording_probe)
endif()
