if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/node_traversal.cpp")
endif()
