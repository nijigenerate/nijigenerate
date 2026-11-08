if(TARGET nijigenerate_debug AND TARGET nijigenerate_renderer)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/debug_gl.cpp")
    target_link_libraries(nijigenerate_renderer PUBLIC nijigenerate_debug)
    target_compile_definitions(nijigenerate_renderer PRIVATE NIJIGENERATE_GL_DEBUG_PROVIDER=1)
endif()
