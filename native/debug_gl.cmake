if(TARGET nijigenerate_debug AND TARGET nijigenerate_gl_backend_source)
    target_sources(nijigenerate_gl_backend_source PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/debug_gl.cpp")
    target_link_libraries(nijigenerate_gl_backend_source PUBLIC nijigenerate_debug)
    target_compile_definitions(nijigenerate_gl_backend_source PRIVATE NIJIGENERATE_GL_DEBUG_PROVIDER=1)
endif()
