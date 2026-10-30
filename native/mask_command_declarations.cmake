if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/mask_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/welding_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_parameter_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/simplephysics_config_declarations.cpp")
endif()
