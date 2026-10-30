if(NIJIGENERATE_IMGUI)
    add_library(nijigenerate_core_package STATIC src/nijigenerate/core.cpp)
    target_link_libraries(nijigenerate_core_package PUBLIC nijigenerate_imgui_sdl3 nijigenerate_native_options)
endif()
