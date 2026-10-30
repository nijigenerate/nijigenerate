if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_mesh_editor_aggregate AND
        TARGET nijigenerate_spline AND NOT TARGET nijigenerate_mesh_editor_operations_impl)
    add_library(nijigenerate_mesh_editor_operations_impl STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/impl.cpp")
    target_include_directories(nijigenerate_mesh_editor_operations_impl PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_mesh_editor_operations_impl PUBLIC cxx_std_20)
    target_sources(nijigenerate_mesh_editor_operations_impl PUBLIC
        FILE_SET mesh_editor_operations_impl_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/impl.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/operations/impl_predicates.hpp")
    target_link_libraries(nijigenerate_mesh_editor_operations_impl PUBLIC nijigenerate_runtime_core
        nijigenerate_mesh_editor_aggregate nijigenerate_spline nijigenerate_debug)
    if(MSVC)
        target_compile_options(nijigenerate_mesh_editor_operations_impl PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_mesh_editor_operations_impl PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
