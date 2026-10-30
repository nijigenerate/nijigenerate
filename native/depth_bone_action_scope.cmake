include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone action scope after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_action_scope.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_action_scope.hpp")
