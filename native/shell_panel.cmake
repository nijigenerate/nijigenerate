if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/shell/shell.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET shell_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/shell/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/shell/shell.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels)
endif()
