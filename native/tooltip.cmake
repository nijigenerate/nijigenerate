if(NIJIGENERATE_IMGUI)
    add_library(nijigenerate_tooltip_declarations INTERFACE)
    target_include_directories(nijigenerate_tooltip_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_tooltip_declarations INTERFACE nijigenerate_native_options)
    target_sources(nijigenerate_tooltip_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/tooltip.hpp")
    add_library(nijigenerate_tooltip STATIC src/nijigenerate/widgets/tooltip.cpp)
    target_link_libraries(nijigenerate_tooltip PUBLIC nijigenerate_tooltip_declarations
        nijigenerate_imgui nijigenerate_core_package)
    if(BUILD_TESTING)
        add_library(nijigenerate_tooltip_header_probe OBJECT tests/tooltip_header_probe.cpp)
        target_link_libraries(nijigenerate_tooltip_header_probe PRIVATE nijigenerate_tooltip_declarations)
        add_executable(nijigenerate_tooltip_recording_probe tests/tooltip_recording_probe.cpp
            src/nijigenerate/widgets/tooltip.cpp src/nijigenerate/core.cpp)
        target_include_directories(nijigenerate_tooltip_recording_probe PRIVATE tests/tooltip_recording src)
        target_link_libraries(nijigenerate_tooltip_recording_probe PRIVATE nijigenerate_native_options)
        add_executable(nijigenerate_tooltip_imgui_probe tests/tooltip_imgui_probe.cpp)
        target_link_libraries(nijigenerate_tooltip_imgui_probe PRIVATE nijigenerate_tooltip)
        add_test(NAME native_tooltip_imgui_contract COMMAND nijigenerate_tooltip_imgui_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_D_IMGUI_SOURCE_DIR)
            add_test(NAME native_tooltip_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_tooltip.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                    --native "$<TARGET_FILE:nijigenerate_tooltip_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/tooltip-evidence/$<CONFIG>")
        endif()
        add_custom_target(nijigenerate_tooltip_project DEPENDS nijigenerate_tooltip_header_probe
            nijigenerate_tooltip nijigenerate_tooltip_recording_probe nijigenerate_tooltip_imgui_probe)
    endif()
endif()
