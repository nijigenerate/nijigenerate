if(NIJIGENERATE_IMGUI)
    include("${CMAKE_CURRENT_LIST_DIR}/viewport_base_primitives.cmake")
    add_library(nijigenerate_viewport_animation STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/anim/package.cpp")
    target_include_directories(nijigenerate_viewport_animation PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_viewport_animation PUBLIC
        FILE_SET viewport_animation_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/anim/package.hpp")
    target_compile_features(nijigenerate_viewport_animation PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_viewport_animation PUBLIC
        nijigenerate_viewport_base_primitives nijigenerate_runtime_core nicxlive::nicxlive)
endif()
