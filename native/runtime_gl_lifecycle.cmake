if(TARGET nijigenerate_renderer AND TARGET nijigenerate_debug)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_queue.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/runtime_gl_lifecycle.cpp")
endif()
