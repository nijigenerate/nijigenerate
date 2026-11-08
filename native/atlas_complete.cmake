# Original atlas implementation uses the current OpenGL context and real texture backend.
if(TARGET nijigenerate_renderer AND TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_renderer PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas_mapping.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/atlas/atlas.cpp")
    target_link_libraries(nijigenerate_renderer PUBLIC
        nijigenerate_core nijigenerate_runtime_core)
endif()
