add_library(nijigenerate_dragdrop STATIC src/nijigenerate/widgets/dragdrop_context.cpp)
target_include_directories(nijigenerate_dragdrop PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_dragdrop PUBLIC cxx_std_20)
target_sources(nijigenerate_dragdrop PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/dragdrop.hpp")
target_link_libraries(nijigenerate_dragdrop PUBLIC nijigenerate_imgui nijigenerate_native_options)
if(BUILD_TESTING)
    add_library(nijigenerate_dragdrop_header_probe OBJECT tests/dragdrop_header_probe.cpp)
    target_link_libraries(nijigenerate_dragdrop_header_probe PRIVATE nijigenerate_dragdrop)
    add_executable(nijigenerate_dragdrop_imgui_probe tests/dragdrop_imgui_probe.cpp)
    target_link_libraries(nijigenerate_dragdrop_imgui_probe PRIVATE nijigenerate_dragdrop)
    add_test(NAME native_dragdrop_imgui_contract COMMAND nijigenerate_dragdrop_imgui_probe)
    add_executable(nijigenerate_dragdrop_recording_probe tests/dragdrop_recording_probe.cpp
        src/nijigenerate/widgets/dragdrop_context.cpp)
    target_include_directories(nijigenerate_dragdrop_recording_probe PRIVATE tests/dragdrop_recording src)
    target_link_libraries(nijigenerate_dragdrop_recording_probe PRIVATE nijigenerate_native_options)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_dragdrop_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_button_color.py"
                --widget dragdrop --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_dragdrop_recording_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/dragdrop-differential/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_dragdrop_project DEPENDS nijigenerate_dragdrop
        nijigenerate_dragdrop_header_probe nijigenerate_dragdrop_imgui_probe nijigenerate_dragdrop_recording_probe)
endif()
