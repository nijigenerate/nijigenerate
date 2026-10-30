# Original atlas implementation uses the current OpenGL context and real texture backend.
if(TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_gl_backend_source PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas_mapping.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas.cpp")
    target_link_libraries(nijigenerate_gl_backend_source PUBLIC
        nijigenerate_core nijigenerate_runtime_core)
endif()
