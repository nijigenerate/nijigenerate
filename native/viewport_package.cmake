if(TARGET nijigenerate_main_viewport AND TARGET nijigenerate_automesh_alpha_provider
        AND NOT TARGET nijigenerate_viewport_package)
    add_library(nijigenerate_viewport_package STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/package.cpp")
    target_sources(nijigenerate_viewport_package PUBLIC
        FILE_SET viewport_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/package.hpp")
    target_include_directories(nijigenerate_viewport_package PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_viewport_package PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_viewport_package PUBLIC nijigenerate_main_viewport
        PRIVATE nijigenerate_automesh_alpha_provider)
endif()
