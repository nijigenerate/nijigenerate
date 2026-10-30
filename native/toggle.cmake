add_library(nijigenerate_toggle STATIC src/nijigenerate/widgets/toggle.cpp)
target_include_directories(nijigenerate_toggle PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_toggle PUBLIC cxx_std_20)
target_sources(nijigenerate_toggle PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/toggle.hpp")
target_link_libraries(nijigenerate_toggle PUBLIC nijigenerate_imgui nijigenerate_native_options)
if(BUILD_TESTING)
    add_executable(nijigenerate_toggle_imgui_probe tests/toggle_imgui_probe.cpp)
    target_link_libraries(nijigenerate_toggle_imgui_probe PRIVATE nijigenerate_toggle)
    add_test(NAME native_toggle_imgui_contract COMMAND nijigenerate_toggle_imgui_probe)
    add_executable(nijigenerate_toggle_recording_probe tests/toggle_recording_probe.cpp src/nijigenerate/widgets/toggle.cpp)
    target_include_directories(nijigenerate_toggle_recording_probe PRIVATE tests/toggle_recording src)
    target_link_libraries(nijigenerate_toggle_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_toggle_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget toggle --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_toggle_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/toggle-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_toggle_project DEPENDS nijigenerate_toggle
        nijigenerate_toggle_imgui_probe nijigenerate_toggle_recording_probe)
endif()
