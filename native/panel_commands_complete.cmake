if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/toggle_panel_command.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel_registry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel_completion.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET panel_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel_key.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel_command_map.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/toggle_panel_command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/view/panel_registry.hpp")
endif()
