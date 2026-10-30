if(TARGET nijigenerate_nagscreen AND TARGET nijigenerate_modal
        AND TARGET nijigenerate_core AND TARGET nijigenerate_link
        AND TARGET nijigenerate_label AND TARGET nijigenerate_dummy
        AND TARGET nijigenerate_button)
    target_sources(nijigenerate_nagscreen PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal/nagscreen_body.cpp")
    target_link_libraries(nijigenerate_nagscreen PUBLIC nijigenerate_modal
        nijigenerate_core nijigenerate_link nijigenerate_label nijigenerate_dummy nijigenerate_button)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal/nagscreen_body.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/modal/nagscreen_body.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
