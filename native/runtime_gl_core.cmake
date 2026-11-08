# Independent OpenGL runtime/provider bodies. The application explicitly selects
# this backend; creating an ImGui device does not silently replace Queue mode.
if(TARGET nijigenerate_renderer AND TARGET nijigenerate_debug)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_shader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_runtime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_backend.cpp")
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_scene_shader_assets.hpp")
    target_link_libraries(nijigenerate_renderer PUBLIC nijigenerate_debug)
    target_sources(nijigenerate_renderer PUBLIC
        FILE_SET runtime_gl_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_backend.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_runtime.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_shader.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_handles.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_renderer PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_renderer PRIVATE -ffp-contract=off)
    endif()
endif()
