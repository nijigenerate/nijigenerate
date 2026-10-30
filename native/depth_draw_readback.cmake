# This original compares supplied value arrays only. It needs the actual
# ComposeResult declaration, not the scene/composer execution producer.
if(TARGET nijigenerate_core AND NOT TARGET nijigenerate_depth_draw_readback)
    add_library(nijigenerate_depth_draw_readback STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/readback.cpp")
    target_include_directories(nijigenerate_depth_draw_readback PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_depth_draw_readback PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_depth_draw_readback PUBLIC nijigenerate_core)
    target_sources(nijigenerate_depth_draw_readback PUBLIC FILE_SET depth_draw_readback_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/readback.hpp")
    if(MSVC)
        target_compile_options(nijigenerate_depth_draw_readback PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_depth_draw_readback PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_depth_draw_readback)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_depth_draw_readback)
endif()
