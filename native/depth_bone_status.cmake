include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone status after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_status.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/homogeneous.hpp")
target_sources(nijigenerate_runtime_core PUBLIC
    FILE_SET depth_bone_status_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_status.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_status_types.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_status_fwd.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/retained_vec3_array.hpp")
