if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/resource.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET resource_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/resource.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels)
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/resource.cpp"
            PROPERTIES COMPILE_OPTIONS "/fp:strict")
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/resource.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
