if(BUILD_TESTING AND TARGET nijigenerate_runtime_core)
    foreach(mode IN ITEMS enabled disabled locked osx)
        set(probe "nijigenerate_dpi_getter_bits_${mode}_probe")
        add_executable(${probe} "${CMAKE_CURRENT_LIST_DIR}/tests/dpi_getter_bits_probe.cpp")
        target_include_directories(${probe} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_link_libraries(${probe} PRIVATE nijigenerate_runtime_core nijigenerate_native_options)
        target_compile_features(${probe} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${probe} PRIVATE /fp:strict /Z7)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${probe} PRIVATE -ffp-contract=off -fno-fast-math)
        endif()
        if(mode STREQUAL "enabled" OR mode STREQUAL "osx")
            target_compile_definitions(${probe} PRIVATE NG_USE_UI_SCALING)
        elseif(mode STREQUAL "locked")
            target_compile_definitions(${probe} PRIVATE NG_NO_UI_SCALING)
        endif()
        if(mode STREQUAL "osx" OR APPLE)
            target_compile_definitions(${probe} PRIVATE NG_OSX)
        endif()
        if(WIN32)
            add_custom_command(TARGET ${probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${probe}> $<TARGET_FILE_DIR:${probe}>
                COMMAND_EXPAND_LISTS)
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            get_filename_component(dpi_bits_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
            get_filename_component(dpi_bits_compiler_root "${dpi_bits_compiler_bin}" DIRECTORY)
            add_test(NAME native_dpi_getter_bits_${mode}_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_dpi_getter_bits.py"
                    --native "$<TARGET_FILE:${probe}>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/dpi-getter-bits-evidence/$<CONFIG>/${mode}"
                    --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --phobos-root "${dpi_bits_compiler_root}/import" --mode "${mode}")
            set_tests_properties(native_dpi_getter_bits_${mode}_differential
                PROPERTIES SKIP_RETURN_CODE 77)
        endif()
    endforeach()
endif()
