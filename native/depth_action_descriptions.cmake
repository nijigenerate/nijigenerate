if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depth.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depth_constructors.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depth_descriptions.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_action_module_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/depth.hpp")
endif()
