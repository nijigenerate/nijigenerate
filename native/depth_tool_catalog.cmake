if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/base_ui.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/plane.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/ring.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/package.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_tool_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/base.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_tool_catalog_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/package.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_plane_tool_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/plane.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_ring_tool_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/ring.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_input nijigenerate_button nijigenerate_drag nijigenerate_tooltip)
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/plane.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/ring.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/plane.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/tools/ring.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
