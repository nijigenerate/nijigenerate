if(TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_debug)
    target_sources(nijigenerate_gl_backend_source PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_queue.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_lifecycle.cpp")
endif()
