# All bodies use the actual common descriptors/math declarations and no renderer calls.
if(TARGET nijigenerate_core AND TARGET nicxlive::common AND NOT TARGET nijigenerate_depth_draw_fitgap)
    add_library(nijigenerate_depth_draw_fitgap STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/fitgap.cpp")
    target_include_directories(nijigenerate_depth_draw_fitgap PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_depth_draw_fitgap PUBLIC nijigenerate_core nicxlive::common)
    target_sources(nijigenerate_depth_draw_fitgap PUBLIC FILE_SET depth_draw_fitgap_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/fitgap.hpp")
    target_compile_features(nijigenerate_depth_draw_fitgap PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_depth_draw_fitgap PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_depth_draw_fitgap PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_depth_draw_fitgap)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_depth_draw_fitgap)
endif()
