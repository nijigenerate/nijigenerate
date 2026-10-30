if(TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_runtime_core
    AND TARGET nijigenerate_core AND NOT TARGET nijigenerate_atlas)
    add_library(nijigenerate_atlas INTERFACE)
    target_sources(nijigenerate_atlas INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas_mapping.hpp")
    target_include_directories(nijigenerate_atlas INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_atlas INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_atlas INTERFACE
        nijigenerate_gl_backend_source nijigenerate_runtime_core nijigenerate_core)
endif()
