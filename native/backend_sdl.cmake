if(TARGET nijigenerate_imgui AND TARGET SDL3::SDL3 AND NOT TARGET nijigenerate_backend_sdl)
    add_library(nijigenerate_backend_sdl INTERFACE)
    target_sources(nijigenerate_backend_sdl INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/sdl.hpp")
    target_include_directories(nijigenerate_backend_sdl INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_backend_sdl INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_backend_sdl INTERFACE nijigenerate_imgui SDL3::SDL3)
endif()
