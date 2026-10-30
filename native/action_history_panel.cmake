# Reuse the actual Panel framework and shared MCP provider; no task-policy shim.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_widget_window
        AND TARGET nijigenerate_mcp_queue AND TARGET nijigenerate_imgui)
    if(NOT TARGET nijigenerate_panels)
        include("${CMAKE_CURRENT_LIST_DIR}/panels.cmake")
        ng_add_panels(nijigenerate_mcp_queue)
    endif()
    add_library(nijigenerate_action_history_panel STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/actionhistory.cpp")
    target_include_directories(nijigenerate_action_history_panel PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_action_history_panel PUBLIC cxx_std_20)
    target_sources(nijigenerate_action_history_panel PUBLIC
        FILE_SET action_history_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/actionhistory.hpp")
    target_link_libraries(nijigenerate_action_history_panel PUBLIC nijigenerate_panels
        nijigenerate_runtime_core nijigenerate_native_options
        PRIVATE nijigenerate_button nijigenerate_dummy nijigenerate_label)
endif()
