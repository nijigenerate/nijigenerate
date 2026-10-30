if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/manifest.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_draw_manifest_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/manifest.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_draw_coordinate_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/coordinate.hpp")
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/manifest.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/coordinate.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/manifest.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/coordinate.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
