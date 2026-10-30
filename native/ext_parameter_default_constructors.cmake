if(BUILD_TESTING AND TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    add_executable(nijigenerate_ext_parameter_default_constructors_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/ext_parameter_default_constructors_probe.cpp")
    target_include_directories(nijigenerate_ext_parameter_default_constructors_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_ext_parameter_default_constructors_probe PRIVATE
        nijigenerate_runtime_core nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_ext_parameter_default_constructors_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_ext_parameter_default_constructors_probe PRIVATE /Z7)
    endif()
    if(WIN32)
        add_custom_command(TARGET nijigenerate_ext_parameter_default_constructors_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_ext_parameter_default_constructors_probe>
                $<TARGET_FILE_DIR:nijigenerate_ext_parameter_default_constructors_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON
       AND NIJIGENERATE_INMATH_SOURCE_DIR AND NIJIGENERATE_PUPPET_SOURCE
       AND NIJIGENERATE_NICXLIVE_SOURCE_DIR)
        get_filename_component(default_ctors_core_dir "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
        get_filename_component(default_ctors_d_root "${default_ctors_core_dir}" DIRECTORY)
        get_filename_component(default_ctors_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        get_filename_component(default_ctors_compiler_root "${default_ctors_compiler_bin}" DIRECTORY)
        add_test(NAME native_ext_parameter_default_constructors_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_ext_parameter_default_constructors.py"
                --native "$<TARGET_FILE:nijigenerate_ext_parameter_default_constructors_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/ext-parameter-default-constructors-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${default_ctors_compiler_root}/import"
                --d-root "${default_ctors_d_root}"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --native-header "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/param/parameter.hpp")
    endif()
endif()
