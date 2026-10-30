if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthimage_completion.cpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthimage_completion.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthimage_completion.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math -fwrapv)
    endif()
endif()
