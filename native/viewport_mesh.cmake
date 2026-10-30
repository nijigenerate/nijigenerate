if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_debug AND NOT TARGET nijigenerate_viewport_mesh)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh.cpp")
    target_sources(nijigenerate_debug PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh_draw.cpp")
    add_library(nijigenerate_viewport_mesh INTERFACE)
    target_include_directories(nijigenerate_viewport_mesh INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_viewport_mesh INTERFACE cxx_std_20)
    target_sources(nijigenerate_viewport_mesh INTERFACE
        FILE_SET mesh_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh.hpp")
    target_link_libraries(nijigenerate_viewport_mesh INTERFACE nijigenerate_runtime_core nijigenerate_debug)
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh_draw.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesh_draw.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
