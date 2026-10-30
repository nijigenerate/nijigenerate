if(TARGET nijigenerate_window_state)
    target_sources(nijigenerate_window_state PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_public_state.cpp")
endif()
