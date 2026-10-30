if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_vertex_runtime)
    add_library(nijigenerate_vertex_runtime INTERFACE)
    target_include_directories(nijigenerate_vertex_runtime INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_vertex_runtime INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/vertex.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/vertex_proxy.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/vertex_bounds.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/vertex_rect.hpp")
    target_compile_features(nijigenerate_vertex_runtime INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_vertex_runtime INTERFACE nicxlive::nicxlive)
    if(MSVC)
        target_compile_options(nijigenerate_vertex_runtime INTERFACE /fp:strict)
    else()
        target_compile_options(nijigenerate_vertex_runtime INTERFACE -ffp-contract=off -fno-fast-math)
    endif()
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_vertex_runtime)
endif()
