include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone merge after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_debug_log.hpp")
