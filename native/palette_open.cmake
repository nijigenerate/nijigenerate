if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(nijigenerate_palette_open_probe tests/palette_open_probe.cpp)
    target_link_libraries(nijigenerate_palette_open_probe PRIVATE nijigenerate_core Threads::Threads)
    if(MSVC)
        target_compile_options(nijigenerate_palette_open_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_palette_open_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_palette_open.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_palette_open_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/palette-open-evidence/$<CONFIG>")
    endif()
endif()
