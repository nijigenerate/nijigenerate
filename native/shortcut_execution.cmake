if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/persistence.cpp")
endif()

if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_modal AND NIJIGENERATE_IMGUI)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/shortcut/execution.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_modal)
endif()
