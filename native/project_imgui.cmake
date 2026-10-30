# The selected Windows source profile statically links the actual ImGui provider.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imgui)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_imgui.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_imgui)
endif()
