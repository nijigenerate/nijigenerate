include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone flush after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_flush_entry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_flush_dispatch.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_fatal_log.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_flush.hpp")
