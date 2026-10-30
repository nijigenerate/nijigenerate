if(NIJIGENERATE_IMGUI AND TARGET SDL3::SDL3)
    if(NOT TARGET nijigenerate_imgui_sdl3)
        message(FATAL_ERROR "GL backend source requires the actual pinned ImGui SDL3 backend")
    endif()
    add_library(nijigenerate_gl_backend_declarations INTERFACE)
    target_include_directories(nijigenerate_gl_backend_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_sources(nijigenerate_gl_backend_declarations INTERFACE
        FILE_SET backend_gl_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/gl.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/gl_backend_procedure.hpp")
    target_link_libraries(nijigenerate_gl_backend_declarations INTERFACE
        nijigenerate_native_options SDL3::SDL3 nijigenerate_imgui nijigenerate_dpi)
    add_library(nijigenerate_gl_backend_source STATIC
        src/nijigenerate/backend/gl_state.cpp src/nijigenerate/backend/gl_platform_shutdown.cpp
        src/nijigenerate/backend/gl_platform_init.cpp
        src/nijigenerate/backend/gl_init.cpp
        src/nijigenerate/backend/gl_mouse_scale.cpp src/nijigenerate/backend/gl_remember_mouse.cpp
        src/nijigenerate/backend/gl_begin_render.cpp src/nijigenerate/backend/gl_font_texture.cpp
        src/nijigenerate/backend/gl_device_delete.cpp
        src/nijigenerate/backend/gl_device_create.cpp
        src/nijigenerate/backend/gl_platform_frame.cpp
        src/nijigenerate/backend/gl_process_event.cpp
        src/nijigenerate/backend/gl_shutdown.cpp
        src/nijigenerate/backend/gl_new_frame.cpp
        src/nijigenerate/backend/gl_render_state.cpp
        src/nijigenerate/backend/gl_render_draw_data.cpp
        src/nijigenerate/platform/gl_backend_policy.cpp src/nijigenerate/backend/gl_shader_check.cpp
        src/nijigenerate/platform/gl_shader_compile.cpp)
    target_link_libraries(nijigenerate_gl_backend_source PUBLIC nijigenerate_gl_backend_declarations)
    target_link_libraries(nijigenerate_gl_backend_source PRIVATE nijigenerate_imgui_sdl3)
    target_sources(nijigenerate_gl_backend_source PRIVATE src/nijigenerate/backend/gl_input_state.hpp)
    target_sources(nijigenerate_imgui_sdl3 PRIVATE src/nijigenerate/backend/gl_input_state.hpp)
endif()
