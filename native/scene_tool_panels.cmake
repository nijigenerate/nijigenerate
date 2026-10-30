if(TARGET nijigenerate_panels AND TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_main_viewport)
    add_library(nijigenerate_scene_tool_panels STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/scene.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/toolsettings.cpp")
    target_link_libraries(nijigenerate_scene_tool_panels PUBLIC nijigenerate_panels
        nijigenerate_gl_backend_source nijigenerate_main_viewport)
    target_sources(nijigenerate_scene_tool_panels PUBLIC
        FILE_SET scene_tool_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/scene.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/toolsettings.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_scene_tool_panels)
endif()
