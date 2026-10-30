if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imagefmt_writer AND TARGET nijigenerate_autosave)
    add_library(nijigenerate_depth_draw_pngexport STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/pngexport.cpp")
    target_include_directories(nijigenerate_depth_draw_pngexport PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_depth_draw_pngexport PUBLIC
        FILE_SET depth_draw_pngexport_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/pngexport.hpp")
    target_compile_features(nijigenerate_depth_draw_pngexport PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_depth_draw_pngexport PUBLIC nijigenerate_runtime_core
        PRIVATE nijigenerate_imagefmt_writer nijigenerate_autosave nijigenerate_native_options)
endif()
