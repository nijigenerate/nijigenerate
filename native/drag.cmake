add_library(nijigenerate_drag STATIC src/nijigenerate/widgets/drag.cpp)
target_include_directories(nijigenerate_drag PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_drag PUBLIC cxx_std_20)
target_sources(nijigenerate_drag PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/drag.hpp")
target_sources(nijigenerate_drag PRIVATE FILE_SET drag_internal_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/drag_internal.hpp")
target_link_libraries(nijigenerate_drag PUBLIC nijigenerate_imgui nijigenerate_native_options)
if(BUILD_TESTING)
    add_library(nijigenerate_drag_header_probe OBJECT tests/drag_header_probe.cpp)
    target_link_libraries(nijigenerate_drag_header_probe PRIVATE nijigenerate_drag)
    add_executable(nijigenerate_drag_imgui_probe tests/drag_imgui_probe.cpp)
    target_link_libraries(nijigenerate_drag_imgui_probe PRIVATE nijigenerate_drag)
    add_test(NAME native_drag_imgui_contract COMMAND nijigenerate_drag_imgui_probe)
    add_executable(nijigenerate_drag_recording_probe tests/drag_recording_probe.cpp src/nijigenerate/widgets/drag.cpp)
    target_include_directories(nijigenerate_drag_recording_probe PRIVATE tests/drag_recording src)
    target_link_libraries(nijigenerate_drag_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_drag_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget drag --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_drag_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/drag-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_drag_project DEPENDS nijigenerate_drag
        nijigenerate_drag_header_probe nijigenerate_drag_imgui_probe nijigenerate_drag_recording_probe)
endif()
