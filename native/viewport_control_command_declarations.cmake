if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_pair_declarations.cpp")
endif()
