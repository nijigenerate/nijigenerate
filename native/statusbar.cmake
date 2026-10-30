add_library(nijigenerate_statusbar STATIC src/nijigenerate/widgets/statusbar_index.cpp
    src/nijigenerate/widgets/statusbar_items.cpp)
target_include_directories(nijigenerate_statusbar PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_sources(nijigenerate_statusbar PUBLIC
    FILE_SET statusbar_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/statusbar.hpp")
target_link_libraries(nijigenerate_statusbar PUBLIC nijigenerate_native_options)
if(NIJIGENERATE_IMGUI AND TARGET nijigenerate_tasks AND TARGET nijigenerate_window_platform)
    target_sources(nijigenerate_statusbar PRIVATE src/nijigenerate/widgets/statusbar_render.cpp)
    target_link_libraries(nijigenerate_statusbar PRIVATE nijigenerate_imgui nijigenerate_dummy
        nijigenerate_label nijigenerate_widget_window nijigenerate_window_theme
        nijigenerate_tasks nijigenerate_task_notification_status)
endif()
if(BUILD_TESTING)
    add_library(nijigenerate_statusbar_header_probe OBJECT tests/statusbar_header_probe.cpp)
    target_link_libraries(nijigenerate_statusbar_header_probe PRIVATE nijigenerate_statusbar)
    add_executable(nijigenerate_statusbar_primitives_probe tests/statusbar_primitives_probe.cpp)
    target_link_libraries(nijigenerate_statusbar_primitives_probe PRIVATE nijigenerate_statusbar)
    add_executable(nijigenerate_statusbar_state_probe tests/statusbar_state_probe.cpp)
    target_link_libraries(nijigenerate_statusbar_state_probe PRIVATE nijigenerate_statusbar)
    add_custom_target(nijigenerate_statusbar_project DEPENDS nijigenerate_statusbar
        nijigenerate_statusbar_header_probe nijigenerate_statusbar_primitives_probe nijigenerate_statusbar_state_probe)
    if(TARGET nijigenerate_imgui)
        add_executable(nijigenerate_statusbar_sidebar_boundary_probe tests/statusbar_sidebar_boundary_probe.cpp)
        target_link_libraries(nijigenerate_statusbar_sidebar_boundary_probe PRIVATE
            nijigenerate_imgui nijigenerate_native_options)
        add_dependencies(nijigenerate_statusbar_project nijigenerate_statusbar_sidebar_boundary_probe)
        add_test(NAME native_statusbar_sidebar_boundary COMMAND nijigenerate_statusbar_sidebar_boundary_probe)
    endif()
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
        add_test(NAME native_statusbar_primitives_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_statusbar.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --fixture primitives
                --native "$<TARGET_FILE:nijigenerate_statusbar_primitives_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/statusbar-primitives-differential/$<CONFIG>")
        add_test(NAME native_statusbar_state_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_statusbar.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --fixture state
                --native "$<TARGET_FILE:nijigenerate_statusbar_state_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/statusbar-state-differential/$<CONFIG>")
    endif()
endif()
