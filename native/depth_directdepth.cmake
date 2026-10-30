if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/directdepth.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_directdepth_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/directdepth.hpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/directdepth.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/directdepth.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -fno-fast-math -ffp-contract=off)
    endif()
endif()
