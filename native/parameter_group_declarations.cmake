if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/group_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/param_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/paramedit_declarations.cpp")
endif()
