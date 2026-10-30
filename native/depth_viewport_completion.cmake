if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_viewport_base_primitives
        AND TARGET nijigenerate_button AND TARGET nijigenerate_tooltip
        AND TARGET nijigenerate_input AND TARGET nijigenerate_depth_camera)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport_ui.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport_completion.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_viewport_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_viewport_base_primitives nijigenerate_button nijigenerate_tooltip
        nijigenerate_input nijigenerate_depth_camera)
    if(NOT TARGET nijigenerate_depth_viewport)
        add_library(nijigenerate_depth_viewport INTERFACE)
        target_include_directories(nijigenerate_depth_viewport INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_depth_viewport INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_depth_viewport INTERFACE nijigenerate_runtime_core)
    endif()
    if(TARGET nijigenerate_main_viewport AND TARGET nijigenerate_viewport_animation)
        target_link_libraries(nijigenerate_main_viewport PUBLIC nijigenerate_viewport_animation)
    endif()
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport_completion.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/viewport_completion.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
