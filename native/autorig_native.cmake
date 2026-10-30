if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_autorig_framework
        AND TARGET nijigenerate_depth_bone_key_projection
        AND TARGET nijigenerate_autorig_observation AND TARGET nijigenerate_autorig_evidence
        AND TARGET nijigenerate_autorig_controls AND TARGET nijigenerate_autorig_review
        AND TARGET nijigenerate_autorig_physics AND NOT TARGET nijigenerate_autorig_native)
    add_library(nijigenerate_autorig_native STATIC
        src/nijigenerate/autorig/deterministic/native_commands.cpp
        src/nijigenerate/autorig/deterministic/native_grid_scales.cpp
        src/nijigenerate/autorig/deterministic/native_mapping.cpp
        src/nijigenerate/autorig/deterministic/native_material.cpp
        src/nijigenerate/autorig/deterministic/native_numbers.cpp
        src/nijigenerate/autorig/deterministic/native_processor.cpp
        src/nijigenerate/autorig/deterministic/native_signature.cpp
        src/nijigenerate/autorig/deterministic/native_stage_build.cpp
        src/nijigenerate/autorig/deterministic/native_stage_cheek.cpp
        src/nijigenerate/autorig/deterministic/native_stage_context.cpp
        src/nijigenerate/autorig/deterministic/native_stage_controls.cpp
        src/nijigenerate/autorig/deterministic/native_stage_depth.cpp
        src/nijigenerate/autorig/deterministic/native_stage_domains.cpp
        src/nijigenerate/autorig/deterministic/native_stage_features.cpp
        src/nijigenerate/autorig/deterministic/native_stage_groups.cpp
        src/nijigenerate/autorig/deterministic/native_stage_lifecycle.cpp
        src/nijigenerate/autorig/deterministic/native_stage_mesh.cpp
        src/nijigenerate/autorig/deterministic/native_stage_observe.cpp
        src/nijigenerate/autorig/deterministic/native_stage_physics.cpp
        src/nijigenerate/autorig/deterministic/native_stage_render.cpp
        src/nijigenerate/autorig/deterministic/native_stage_review.cpp
        src/nijigenerate/autorig/deterministic/native_stage_uv.cpp
        src/nijigenerate/autorig/deterministic/native_stage_verify.cpp
        src/nijigenerate/autorig/deterministic/native_stage_weld.cpp
        src/nijigenerate/autorig/deterministic/native_state.cpp)
    target_include_directories(nijigenerate_autorig_native PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_autorig_native PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/detail/native_memory.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/detail/native_maps.hpp")
    target_compile_features(nijigenerate_autorig_native PUBLIC cxx_std_20)
    target_sources(nijigenerate_autorig_native PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/native.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_base64.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_base64_exception.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/autorig/deterministic/native_stage.hpp")
    target_link_libraries(nijigenerate_autorig_native PUBLIC
        nijigenerate_depth_bone_key_projection
        nijigenerate_runtime_core nijigenerate_autorig_framework
        nijigenerate_autorig_observation nijigenerate_autorig_evidence
        nijigenerate_autorig_controls nijigenerate_autorig_review nijigenerate_autorig_physics)
    if(MSVC)
        target_compile_options(nijigenerate_autorig_native PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_autorig_native PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
