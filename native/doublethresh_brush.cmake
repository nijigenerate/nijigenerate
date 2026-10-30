# Full original DoubleThreshBrush product source; no test/probe option selected.
if(NOT TARGET nijigenerate_doublethresh_brush)
    if(TARGET nijigenerate_mesh_brushes)
        add_library(nijigenerate_doublethresh_brush INTERFACE)
        target_link_libraries(nijigenerate_doublethresh_brush INTERFACE nijigenerate_mesh_brushes)
    elseif(TARGET nijigenerate_viewport_common AND TARGET nijigenerate_debug AND TARGET nijigenerate_drag)
        add_library(nijigenerate_doublethresh_brush STATIC
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/doublethreshbrush.cpp")
        target_include_directories(nijigenerate_doublethresh_brush PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
        target_sources(nijigenerate_doublethresh_brush PUBLIC
            FILE_SET doublethresh_brush_headers TYPE HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/brushes/doublethreshbrush.hpp")
        target_compile_features(nijigenerate_doublethresh_brush PUBLIC cxx_std_20)
        target_link_libraries(nijigenerate_doublethresh_brush PUBLIC
            nijigenerate_viewport_common nijigenerate_debug nijigenerate_drag)
        if(MSVC)
            target_compile_options(nijigenerate_doublethresh_brush PRIVATE /fp:strict)
        else()
            target_compile_options(nijigenerate_doublethresh_brush PRIVATE -ffp-contract=off -fno-fast-math)
        endif()
    endif()
endif()
