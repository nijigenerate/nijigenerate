include_guard(GLOBAL)
if(NOT TARGET nijigenerate_runtime_core)
    message(FATAL_ERROR "Register Depth Bone jobs after the actual runtime core target")
endif()
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_control.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_workers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_freshness.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_packet_build.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_gpu_packet.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_job_state.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/bone_job_workers.hpp")
if(MSVC)
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_freshness.cpp"
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_job_freshness.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
endif()
