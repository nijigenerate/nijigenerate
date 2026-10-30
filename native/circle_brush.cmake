# Product-source publication for the complete original CircleBrush module.
# Reuse the actual existing aggregate body when selected; never compile a second
# CircleBrush body alongside it, and do not enable a test/probe configuration.
if(NOT TARGET nijigenerate_circle_brush)
    if(TARGET nijigenerate_mesh_brushes)
        add_library(nijigenerate_circle_brush INTERFACE)
        target_link_libraries(nijigenerate_circle_brush INTERFACE nijigenerate_mesh_brushes)
    elseif(TARGET nijigenerate_viewport_common AND TARGET nijigenerate_debug AND TARGET nijigenerate_drag)
        add_library(nijigenerate_circle_brush STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/circlebrush.cpp")
        target_include_directories(nijigenerate_circle_brush PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_sources(nijigenerate_circle_brush PUBLIC
            FILE_SET circle_brush_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/circlebrush.hpp")
        target_compile_features(nijigenerate_circle_brush PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_circle_brush PUBLIC
            nijigenerate_viewport_common nijigenerate_debug nijigenerate_drag)
        if(MSVC)
            target_compile_options(nijigenerate_circle_brush PRIVATE /fp:strict)
        else()
            target_compile_options(nijigenerate_circle_brush PRIVATE -ffp-contract=off -fno-fast-math)
        endif()
    endif()
endif()
