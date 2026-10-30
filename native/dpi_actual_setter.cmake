# Stage as native/dpi_actual_setter.cmake after shared input release.
# Copy the other files in this stage to native/tests/dpi-actual-setter/.
if(BUILD_TESTING)
    foreach(mode enabled disabled locked osx)
        set(probe "nijigenerate_dpi_actual_setter_${mode}_probe")
        add_executable(${probe} tests/dpi-actual-setter/dpi_actual_setter_probe.cpp
            src/nijigenerate/core/dpi.cpp)
        target_link_libraries(${probe} PRIVATE nijigenerate_runtime_core)
        if(mode STREQUAL "enabled" OR mode STREQUAL "osx")
            target_compile_definitions(${probe} PRIVATE NG_USE_UI_SCALING)
        elseif(mode STREQUAL "locked")
            target_compile_definitions(${probe} PRIVATE NG_NO_UI_SCALING)
        endif()
        if(mode STREQUAL "osx")
            target_compile_definitions(${probe} PRIVATE NG_OSX)
        endif()
        add_dependencies(nijigenerate_dpi_project ${probe})
        if(WIN32)
            add_custom_command(TARGET ${probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${probe}> $<TARGET_FILE_DIR:${probe}>
                COMMAND_EXPAND_LISTS)
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            get_filename_component(dpi_actual_d_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
            add_test(NAME native_dpi_actual_setter_${mode}_differential
                COMMAND "${NIJIGENERATE_PYTHON}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/dpi-actual-setter/compare_dpi_actual_setter.py"
                    --root "${CMAKE_CURRENT_SOURCE_DIR}/.."
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:${probe}>"
                    --phobos-root "${dpi_actual_d_bin}/../import"
                    --mode ${mode}
                    --output "${CMAKE_CURRENT_BINARY_DIR}/dpi-actual-setter-evidence/$<CONFIG>/${mode}")
            set_tests_properties(native_dpi_actual_setter_${mode}_differential PROPERTIES SKIP_RETURN_CODE 77)
        endif()
    endforeach()
endif()
