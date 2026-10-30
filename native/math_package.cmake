if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET math_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET triangle_module_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle_fill.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle_resampling.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangulate.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle_bounds.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle_sign.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/triangle_grid.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/vertex.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangle_coordinates.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangle_mesh_resampling.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/grid_locate_interval.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/grid_bilinear.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangle_fill_scalar.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangulation_split_edges.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangulation_captured.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangulation_adjacency.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/triangulation_predicates.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/detail/delaunay_diagnostics.hpp")
endif()
