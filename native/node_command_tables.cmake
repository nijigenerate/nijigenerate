if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/mask.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/welding.cpp")
endif()
