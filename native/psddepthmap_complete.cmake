# Independent psddepthmap.d whole dialog implementation. Runtime validation is deferred.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_gl_backend_source AND
        TARGET nijigenerate_depth_draw_gpu AND TARGET nijigenerate_dialogs AND
        TARGET nijigenerate_io AND TARGET nijigenerate_tooltip AND TARGET nijigenerate_texture_widget)
    set(_psddepthmap_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_canvas.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_dialog.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_edit_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_gpu.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_mapping_ui.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_options.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_preview.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_regression.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_samples.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_window.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_psddepthmap_sources})
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_complete_values.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/psddepthmap_values.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/detail/psddepthmap_value_declarations.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_gl_backend_source
        nijigenerate_depth_draw_gpu nijigenerate_depth_camera nijigenerate_dialogs
        nijigenerate_label nijigenerate_button nijigenerate_dummy nijigenerate_tooltip
        nijigenerate_texture_widget nijigenerate_io OpenGL::GL)
    if(MSVC)
        set_property(SOURCE ${_psddepthmap_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_psddepthmap_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_psddepthmap_sources)
endif()
