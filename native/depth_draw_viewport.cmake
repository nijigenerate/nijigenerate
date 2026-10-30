if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_debug AND
        TARGET nijigenerate_viewport_base_primitives AND TARGET nijigenerate_depth_camera
        AND TARGET nijigenerate_depth_draw_gpu AND TARGET nijigenerate_button
        AND TARGET nijigenerate_label AND TARGET nijigenerate_viewport AND TARGET nijigenerate_input)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_state.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_geometry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_render.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_draw_viewport_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_debug nijigenerate_viewport_base_primitives nijigenerate_depth_camera
        nijigenerate_depth_draw_gpu
        nijigenerate_button nijigenerate_label nijigenerate_viewport nijigenerate_input)
    if(NOT TARGET nijigenerate_depth_draw_viewport)
        add_library(nijigenerate_depth_draw_viewport INTERFACE)
        target_include_directories(nijigenerate_depth_draw_viewport INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_depth_draw_viewport INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_depth_draw_viewport INTERFACE nijigenerate_runtime_core)
    endif()
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_state.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_geometry.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_render.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_state.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_geometry.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/viewport_render.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
