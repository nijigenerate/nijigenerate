if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/mesh_vertices.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET mesh_application_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/mesh.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/mesh_declarations.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/mesh_vertices.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/mesh_apply.hpp")
endif()
