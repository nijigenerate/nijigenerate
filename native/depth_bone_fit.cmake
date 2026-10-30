include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone fitting after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_source_geometry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_depth_fit.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_fit_entry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_rest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_fit.hpp")
if(MSVC)
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_rest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_source_geometry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_depth_fit.cpp"
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_rest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_source_geometry.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_depth_fit.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
endif()
target_sources(nijigenerate_runtime_core PUBLIC
    FILE_SET depth_bone_fit_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_commands.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_constants.hpp")
