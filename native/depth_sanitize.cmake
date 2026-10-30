if(BUILD_TESTING)
    add_executable(nijigenerate_depth_sanitize_probe tests/depth_sanitize_probe.cpp)
    target_link_libraries(nijigenerate_depth_sanitize_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depth_path_probe tests/depth_path_probe.cpp)
    target_link_libraries(nijigenerate_depth_path_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depth_path_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_path.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_path_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-path-evidence/$<CONFIG>")
        add_test(NAME native_depth_sanitize_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_sanitize.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_sanitize_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-sanitize-evidence/$<CONFIG>")
    endif()
endif()
