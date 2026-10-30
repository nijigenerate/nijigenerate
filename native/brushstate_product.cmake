# Complete original teacher-state product source. Include after the legacy
# aggregate policy; do not turn on a probe/test option to expose this body.
if(TARGET nijigenerate_window_theme AND TARGET nijigenerate_window_declarations
        AND TARGET nicxlive::nicxlive)
    if(NOT TARGET nijigenerate_window_background)
        add_library(nijigenerate_window_background STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_background.cpp")
        target_include_directories(nijigenerate_window_background PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_window_background PUBLIC cxx_std_20)
        target_sources(nijigenerate_window_background PRIVATE FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/window_background_state.hpp")
        target_link_libraries(nijigenerate_window_background PUBLIC
            nijigenerate_window_declarations nijigenerate_window_theme nicxlive::nicxlive)
    endif()
    if(NOT TARGET nijigenerate_brushstate)
        add_library(nijigenerate_brushstate STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushstate.cpp")
        target_include_directories(nijigenerate_brushstate PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_brushstate PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_brushstate PUBLIC nijigenerate_window_background)
    endif()
    # Publish the same header when reusing the legacy real product target too.
    get_target_property(ng_brushstate_published nijigenerate_brushstate NG_BRUSHSTATE_HEADER_PUBLISHED)
    if(NOT ng_brushstate_published)
        target_sources(nijigenerate_brushstate PUBLIC
            FILE_SET brushstate_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushstate.hpp")
        set_property(TARGET nijigenerate_brushstate PROPERTY NG_BRUSHSTATE_HEADER_PUBLISHED TRUE)
    endif()
    unset(ng_brushstate_published)
endif()
