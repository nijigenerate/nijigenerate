if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/camera.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET camera_action_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/camera.hpp")
    if(TARGET nijigenerate_excamera)
        target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_excamera)
    endif()
endif()
