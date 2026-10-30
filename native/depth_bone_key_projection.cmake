if(TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_core AND TARGET nijigenerate_depthbone_nodes
        AND TARGET nicxlive::nicxlive
        AND NOT TARGET nijigenerate_depth_bone_key_projection)
    add_library(nijigenerate_depth_bone_key_projection STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_key_projection.cpp")
    target_include_directories(nijigenerate_depth_bone_key_projection PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_depth_bone_key_projection PUBLIC cxx_std_20)
    target_sources(nijigenerate_depth_bone_key_projection PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_key_projection.hpp")
    target_link_libraries(nijigenerate_depth_bone_key_projection PUBLIC
        nijigenerate_runtime_core nijigenerate_core nijigenerate_depthbone_nodes nicxlive::nicxlive)
    if(MSVC)
        target_compile_options(nijigenerate_depth_bone_key_projection PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_depth_bone_key_projection PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
