if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_hierarchy_actions.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_replace_actions.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/impl_welding.cpp")
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_hierarchy_actions.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_replace_actions.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_hierarchy_actions.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/node_replace_actions.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
