if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_binding_points_probe tests/binding_points_probe.cpp)
    target_link_libraries(nijigenerate_binding_points_probe PRIVATE nijigenerate_core Threads::Threads)
    if(MSVC)
        target_compile_options(nijigenerate_binding_points_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR)
        add_test(NAME native_binding_points_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_binding_points.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --native "$<TARGET_FILE:nijigenerate_binding_points_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/binding-points-evidence/$<CONFIG>")
    endif()
endif()
