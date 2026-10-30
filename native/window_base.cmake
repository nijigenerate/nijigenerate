# Include after widget_window.cmake and mcp_queue.cmake.
if(TARGET nijigenerate_widget_window AND TARGET nijigenerate_mcp_queue)
    add_library(nijigenerate_window_base STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/base.cpp")
    target_include_directories(nijigenerate_window_base PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_window_base PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/base.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/detail/source_class_info.hpp")
    target_compile_definitions(nijigenerate_window_base PRIVATE NG_USE_UI_SCALING)
    target_link_libraries(nijigenerate_window_base PUBLIC nijigenerate_widget_window nicxlive::nicxlive
        PRIVATE nijigenerate_mcp_queue nijigenerate_core)

    if(TARGET nijigenerate_widget_window_no_scaling AND TARGET nijigenerate_window_platform)
        add_library(nijigenerate_window_base_no_scaling STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/base.cpp")
        target_include_directories(nijigenerate_window_base_no_scaling PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_sources(nijigenerate_window_base_no_scaling PUBLIC FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/base.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/detail/source_class_info.hpp")
        target_compile_definitions(nijigenerate_window_base_no_scaling PRIVATE NG_NO_UI_SCALING)
        target_link_libraries(nijigenerate_window_base_no_scaling PUBLIC nijigenerate_widget_window_no_scaling nicxlive::nicxlive
            PRIVATE nijigenerate_mcp_queue nijigenerate_window_platform nijigenerate_core)
    endif()
endif()
