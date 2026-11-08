# Vulkan shader assets are compiled locally; configuration never downloads an SDK.
find_package(Vulkan REQUIRED COMPONENTS glslc)
set(_ng_vk_sources "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/vulkan")
set(_ng_vk_generated "${CMAKE_CURRENT_BINARY_DIR}/vulkan")
set(_ng_vk_headers)
set(_ng_vk_includes "#pragma once\n#include <cstdint>\n")
foreach(shader part.vert part.frag part_blend.frag part_bump.frag part_mrt.frag part_mrt_mask.frag lighting.frag part_stage1.frag part_stage2.frag part_mask.frag
        mask.vert mask.frag overlay_blur.frag emission_combine.frag
        opaque_alpha.vert opaque_alpha.frag depth_bone.comp depth_sample.comp difference.comp)
    string(REPLACE "." "_" symbol "${shader}")
    set(spv "${_ng_vk_generated}/${shader}.spv")
    set(header "${_ng_vk_generated}/${symbol}.hpp")
    add_custom_command(OUTPUT "${header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ng_vk_generated}"
        COMMAND Vulkan::glslc "${_ng_vk_sources}/shaders/${shader}" -o "${spv}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${spv}" "-DOUTPUT=${header}" "-DSYMBOL=${symbol}"
            -P "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        DEPENDS "${_ng_vk_sources}/shaders/${shader}" "${PROJECT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        VERBATIM)
    list(APPEND _ng_vk_headers "${header}")
    string(APPEND _ng_vk_includes "#include \"vulkan/${symbol}.hpp\"\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vulkan_shaders.hpp" "${_ng_vk_includes}")
include("${CMAKE_CURRENT_LIST_DIR}/imgui_vulkan.cmake")
add_library(nijigenerate_vulkan STATIC
    "${_ng_vk_sources}/context.cpp" "${_ng_vk_sources}/backend.cpp" "${_ng_vk_sources}/emitter.cpp"
    "${_ng_vk_sources}/compute.cpp" "${_ng_vk_sources}/difference.cpp" ${_ng_vk_headers}
    "${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_vulkan_editor.cpp")
target_include_directories(nijigenerate_vulkan PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
    "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(nijigenerate_vulkan PUBLIC nicxlive::nicxlive
    nijigenerate_imgui SDL3::SDL3 Vulkan::Vulkan)

target_link_libraries(nijigenerate_depth_gpu_render PUBLIC nijigenerate_vulkan)
target_link_libraries(nijigenerate_depth_draw_gpu PUBLIC nijigenerate_vulkan)
if(APPLE)
    get_filename_component(_ng_vk_library_dir "${Vulkan_LIBRARY}" DIRECTORY)
    get_filename_component(_ng_vk_sdk_prefix "${_ng_vk_library_dir}" DIRECTORY)
    find_file(NIJIGENERATE_MOLTENVK_LIBRARY libMoltenVK.dylib
        PATHS "${_ng_vk_library_dir}" NO_DEFAULT_PATH REQUIRED)
    find_file(NIJIGENERATE_MOLTENVK_MANIFEST MoltenVK_icd.json
        PATHS "${_ng_vk_sdk_prefix}/share/vulkan/icd.d" NO_DEFAULT_PATH REQUIRED)
    file(READ "${NIJIGENERATE_MOLTENVK_MANIFEST}" _ng_vk_manifest)
    string(JSON _ng_vk_manifest SET "${_ng_vk_manifest}" ICD library_path
        "\"../../../Frameworks/libMoltenVK.dylib\"")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json" "${_ng_vk_manifest}\n")
    set_property(TARGET nijigenerate APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
    add_custom_command(TARGET nijigenerate POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Frameworks"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Resources/vulkan/icd.d"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${Vulkan_LIBRARY}"
            "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Frameworks/libvulkan.1.dylib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${NIJIGENERATE_MOLTENVK_LIBRARY}"
            "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Frameworks/libMoltenVK.dylib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json"
            "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json"
        VERBATIM)
endif()
