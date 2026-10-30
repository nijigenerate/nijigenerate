add_library(nijigenerate_depth_gpu_nonrender INTERFACE)
target_include_directories(nijigenerate_depth_gpu_nonrender INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(nijigenerate_depth_gpu_nonrender INTERFACE nijigenerate_native_options)
target_compile_definitions(nijigenerate_depth_gpu_nonrender INTERFACE NIJIGENERATE_GPU_NONRENDER=1)
if(TARGET nicxlive::nicxlive)
    target_link_libraries(nijigenerate_depth_gpu_nonrender INTERFACE nicxlive::nicxlive)
endif()

set(ng_depth_bone_gpu_headers
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_gpu_async.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/gpu_dispatch_packet.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/gpu_async_result.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_simple_hook_types.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_simple_hook_slots.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_nonrender_operations.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_render_operations.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_pending_job.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_render_state.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_indexed_binding.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_indexed_binding_operations.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_program.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_vertex_source.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_job_resources.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/detail/gpu_submit.hpp")
target_sources(nijigenerate_depth_gpu_nonrender INTERFACE
    FILE_SET depth_bone_gpu_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" FILES ${ng_depth_bone_gpu_headers})

if(TARGET SDL3::SDL3 AND TARGET nicxlive::nicxlive)
    add_library(nijigenerate_depth_gpu_render STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/bone_gpu_async_render.cpp")
    target_include_directories(nijigenerate_depth_gpu_render PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_depth_gpu_render PUBLIC nijigenerate_native_options SDL3::SDL3 nicxlive::nicxlive)
    target_compile_definitions(nijigenerate_depth_gpu_render PUBLIC NIJIGENERATE_GPU_RENDER=1)
    target_sources(nijigenerate_depth_gpu_render PUBLIC
        FILE_SET depth_bone_gpu_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" FILES ${ng_depth_bone_gpu_headers})
    if(TARGET nijigenerate_runtime_core)
        target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_depth_gpu_render)
    endif()
endif()
