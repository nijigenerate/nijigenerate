add_library(nijigenerate_window_platform STATIC src/nijigenerate/core/window_platform.cpp)
target_include_directories(nijigenerate_window_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(nijigenerate_window_platform PUBLIC nijigenerate_native_options)
if(BUILD_TESTING)
    add_library(nijigenerate_window_wayland_header_probe OBJECT tests/window_wayland_header_probe.cpp)
    target_link_libraries(nijigenerate_window_wayland_header_probe PRIVATE nijigenerate_window_platform)
    add_executable(nijigenerate_window_wayland_probe tests/window_wayland_probe.cpp)
    target_link_libraries(nijigenerate_window_wayland_probe PRIVATE nijigenerate_window_platform)
    add_custom_target(nijigenerate_window_platform_project DEPENDS nijigenerate_window_platform
        nijigenerate_window_wayland_header_probe nijigenerate_window_wayland_probe)
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_window_wayland_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_window_wayland.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_window_wayland_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/window-wayland-differential/$<CONFIG>")
    endif()
endif()
