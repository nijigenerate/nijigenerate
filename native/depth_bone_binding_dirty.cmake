include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone binding dirty helpers after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_binding_dirty.cpp")
