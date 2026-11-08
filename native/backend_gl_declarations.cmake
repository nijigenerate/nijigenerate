if(NIJIGENERATE_IMGUI AND TARGET SDL3::SDL3)
    add_library(nijigenerate_renderer_declarations INTERFACE)
    target_include_directories(nijigenerate_renderer_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_renderer_declarations INTERFACE
        nijigenerate_native_options SDL3::SDL3 nijigenerate_imgui nijigenerate_dpi)
    add_library(nijigenerate_renderer STATIC src/nijigenerate/backend/renderer.cpp)
    target_link_libraries(nijigenerate_renderer PUBLIC nijigenerate_renderer_declarations
        nijigenerate_vulkan nijigenerate_debug PRIVATE nijigenerate_imgui_sdl3)
endif()
