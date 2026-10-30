# Requires the actual public Panel provider to be installed by its owner first.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels AND NOT TARGET nijigenerate_logger_panel)
    add_library(nijigenerate_logger_panel STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/logger.cpp")
    target_link_libraries(nijigenerate_logger_panel PUBLIC nijigenerate_panels)
    target_sources(nijigenerate_logger_panel PUBLIC
        FILE_SET logger_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/logger.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_logger_panel)
endif()
