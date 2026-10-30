if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/unique_psd_path.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_math.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_enum_functions.cpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_math.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthmap_psd_math.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
