# Complete RectangleBrush product publication without selecting a probe/test.
if(NOT TARGET nijigenerate_rectangle_brush)
    if(TARGET nijigenerate_mesh_brushes)
        add_library(nijigenerate_rectangle_brush INTERFACE)
        target_link_libraries(nijigenerate_rectangle_brush INTERFACE nijigenerate_mesh_brushes)
    elseif(TARGET nijigenerate_viewport_common AND TARGET nijigenerate_debug AND TARGET nijigenerate_drag)
        add_library(nijigenerate_rectangle_brush STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/rectanglebrush.cpp")
        target_include_directories(nijigenerate_rectangle_brush PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_sources(nijigenerate_rectangle_brush PUBLIC
            FILE_SET rectangle_brush_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/rectanglebrush.hpp")
        target_compile_features(nijigenerate_rectangle_brush PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_rectangle_brush PUBLIC
            nijigenerate_viewport_common nijigenerate_debug nijigenerate_drag)
        if(MSVC)
            target_compile_options(nijigenerate_rectangle_brush PRIVATE /fp:strict)
        else()
            target_compile_options(nijigenerate_rectangle_brush PRIVATE -ffp-contract=off -fno-fast-math)
        endif()
    endif()
endif()
