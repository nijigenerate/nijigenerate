if(NIJIGENERATE_IMGUI AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imgui
        AND NOT TARGET nijigenerate_depth_camera)
    add_library(nijigenerate_depth_camera STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/camera_input.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/camera_focus.cpp")
    target_sources(nijigenerate_depth_camera PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/camera.hpp")
    target_include_directories(nijigenerate_depth_camera PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_depth_camera PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_depth_camera PUBLIC nijigenerate_runtime_core
        PRIVATE nijigenerate_imgui)
    # The public inline projection routines preserve the original float operation order.
    if(MSVC)
        target_compile_options(nijigenerate_depth_camera PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_depth_camera PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
