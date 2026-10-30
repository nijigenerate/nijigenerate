if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_tasks
        AND TARGET nijigenerate_inpexport_window AND TARGET nijigenerate_imagefmt_reader)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_import.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_ui.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_tasks nijigenerate_inpexport_window
        nijigenerate_imagefmt_reader)
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_import.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_ui.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_import.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_ui.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
