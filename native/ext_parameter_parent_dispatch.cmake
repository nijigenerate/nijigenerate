if(BUILD_TESTING AND TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    add_executable(nijigenerate_ext_parameter_parent_dispatch_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/ext_parameter_parent_dispatch_probe.cpp")
    target_include_directories(nijigenerate_ext_parameter_parent_dispatch_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_ext_parameter_parent_dispatch_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_ext_parameter_parent_dispatch_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_ext_parameter_parent_dispatch_probe PRIVATE /Z7)
    endif()
    if(WIN32)
        add_custom_command(TARGET nijigenerate_ext_parameter_parent_dispatch_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_ext_parameter_parent_dispatch_probe>
                $<TARGET_FILE_DIR:nijigenerate_ext_parameter_parent_dispatch_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        get_filename_component(parent_dispatch_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        get_filename_component(parent_dispatch_compiler_root "${parent_dispatch_compiler_bin}" DIRECTORY)
        add_test(NAME native_ext_parameter_parent_dispatch_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_ext_parameter_parent_dispatch.py"
                --native "$<TARGET_FILE:nijigenerate_ext_parameter_parent_dispatch_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/ext-parameter-parent-dispatch-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${parent_dispatch_compiler_root}/import")
    endif()
endif()
