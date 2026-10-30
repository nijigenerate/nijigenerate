if(BUILD_TESTING AND TARGET nicxlive::nicxlive)
    add_executable(nijigenerate_binding_drag_data_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/binding_drag_data_probe.cpp")
    target_include_directories(nijigenerate_binding_drag_data_probe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_binding_drag_data_probe PRIVATE
        nijigenerate_native_options nicxlive::nicxlive)
    target_compile_features(nijigenerate_binding_drag_data_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_binding_drag_data_probe PRIVATE /Z7)
    endif()
    if(WIN32)
        add_custom_command(TARGET nijigenerate_binding_drag_data_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_binding_drag_data_probe>
                $<TARGET_FILE_DIR:nijigenerate_binding_drag_data_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_NICXLIVE_SOURCE_DIR)
        get_filename_component(drag_data_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
        get_filename_component(drag_data_compiler_root "${drag_data_compiler_bin}" DIRECTORY)
        add_test(NAME native_binding_drag_data_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_binding_drag_data.py"
                --native "$<TARGET_FILE:nijigenerate_binding_drag_data_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/binding-drag-data-evidence/$<CONFIG>"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --phobos-root "${drag_data_compiler_root}/import"
                --native-header "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/param/parameter.hpp")
    endif()
endif()
