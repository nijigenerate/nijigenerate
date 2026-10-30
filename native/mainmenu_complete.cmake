if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/mainmenu.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET mainmenu_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/mainmenu.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_logo nijigenerate_panels nijigenerate_label nijigenerate_dummy nijigenerate_tooltip)
    if(MSVC)
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/mainmenu.cpp"
            PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/mainmenu.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
if(TARGET nijigenerate_logo AND TARGET nijigenerate_imagefmt_reader)
    target_link_libraries(nijigenerate_logo PUBLIC nijigenerate_imagefmt_reader)
endif()
