if(BUILD_TESTING)
    add_executable(nijigenerate_port_enum_values_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/port_enum_values_probe.cpp")
    target_include_directories(nijigenerate_port_enum_values_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_port_enum_values_probe PRIVATE nijigenerate_native_options)
    target_compile_features(nijigenerate_port_enum_values_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_port_enum_values_probe PRIVATE /Z7)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        get_filename_component(enum_values_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        get_filename_component(enum_values_compiler_root "${enum_values_compiler_bin}" DIRECTORY)
        add_test(NAME native_port_enum_values_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_port_enum_values.py"
                --native "$<TARGET_FILE:nijigenerate_port_enum_values_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/port-enum-values-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${enum_values_compiler_root}/import")
    endif()
endif()
