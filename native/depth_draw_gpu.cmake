if(TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_depth_draw_gpu STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu_data.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu_validation.cpp")
    target_sources(nijigenerate_depth_draw_gpu PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu_compose_sort.hpp")
    target_include_directories(nijigenerate_depth_draw_gpu PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_depth_draw_gpu PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu.hpp")
    target_compile_features(nijigenerate_depth_draw_gpu PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_depth_draw_gpu PUBLIC nijigenerate_runtime_core)
    if(TARGET nijigenerate_gl_backend_source)
        target_sources(nijigenerate_depth_draw_gpu PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/gpu_gl.cpp")
        target_compile_definitions(nijigenerate_depth_draw_gpu PRIVATE NIJIGENERATE_DEPTH_GPU_RENDER=1)
        target_link_libraries(nijigenerate_depth_draw_gpu PUBLIC nijigenerate_gl_backend_source)
    endif()
    if(MSVC)
        target_compile_options(nijigenerate_depth_draw_gpu PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_depth_draw_gpu PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
