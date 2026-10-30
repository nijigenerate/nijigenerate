# Call only after the actual shared M029 provider target/header are installed.
# A missing provider is a real integration failure, not a no-op policy fallback.
function(ng_add_panels mcp_provider_target)
    if(NOT TARGET "${mcp_provider_target}")
        message(FATAL_ERROR "M184 requires the actual shared MCP task provider target")
    endif()
    if(NOT TARGET nijigenerate_runtime_core OR NOT TARGET nijigenerate_widget_window)
        message(FATAL_ERROR "M184 requires actual project/settings and widget-window providers")
    endif()
    add_library(nijigenerate_panels STATIC
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/panels/package.cpp")
    target_compile_features(nijigenerate_panels PUBLIC cxx_std_20)
    target_compile_definitions(nijigenerate_panels PRIVATE $<$<CONFIG:Debug>:NG_DEBUG_IMGUI=1>)
    target_sources(nijigenerate_panels PUBLIC
        FILE_SET panel_framework_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/nijigenerate/panels/package.hpp")
    target_link_libraries(nijigenerate_panels PUBLIC nijigenerate_runtime_core
        nijigenerate_widget_window nijigenerate_imgui "${mcp_provider_target}"
        PRIVATE nijigenerate_window_debug)
endfunction()
