include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone refresh state after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_refresh_state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_refresh.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_refresh.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_refresh_state.hpp")
