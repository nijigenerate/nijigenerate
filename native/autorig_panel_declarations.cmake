# Actual public panel declarations only; worker/GUI/manager bodies stay separate.
if(TARGET nijigenerate_panels AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_autorig_workflow AND TARGET nijigenerate_autorig_presentation
        AND NOT TARGET nijigenerate_autorig_panel_declarations)
    add_library(nijigenerate_autorig_panel_declarations INTERFACE)
    target_link_libraries(nijigenerate_autorig_panel_declarations INTERFACE
        nijigenerate_panels nijigenerate_runtime_core nijigenerate_autorig_workflow
        nijigenerate_autorig_presentation)
    target_sources(nijigenerate_autorig_panel_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/autorig.hpp")
endif()
