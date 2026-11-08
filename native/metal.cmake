# Metal sources and checked-in MSL need no Vulkan SDK or online shader tools.
set(_ng_metal_sources "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/backend/metal")
set(_ng_metal_includes "#pragma once\n")
foreach(shader render depth_bone depth_sample difference)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ng_metal_sources}/shaders/${shader}.metal")
    file(READ "${_ng_metal_sources}/shaders/${shader}.metal" _ng_metal_shader)
    string(APPEND _ng_metal_includes "inline constexpr char ${shader}_source[] = R\"NG_METAL(${_ng_metal_shader})NG_METAL\";\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/metal_shaders.hpp" "${_ng_metal_includes}")

# Preserve the pinned ImGui version. Its ordinary textures contain straight
# alpha, while our viewport contains premultiplied alpha.
file(READ "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends/imgui_impl_metal.mm" _ng_imgui_metal)
string(REPLACE "void ImGui_ImplMetal_RenderDrawData(ImDrawData* drawData,"
    "static ImTextureID ngPremultipliedTexture = 0;\nvoid ngImGuiMetalPremultipliedTexture(ImTextureID id) { ngPremultipliedTexture = id; }\nvoid ImGui_ImplMetal_RenderDrawData(ImDrawData* drawData," _ng_imgui_metal "${_ng_imgui_metal}")
string(REPLACE "                // Bind texture, Draw"
    "                const uint32_t ngPremultiplied = pcmd->GetTexID() == ngPremultipliedTexture;\n                [commandEncoder setFragmentBytes:&ngPremultiplied length:sizeof(ngPremultiplied) atIndex:2];\n                // Bind texture, Draw" _ng_imgui_metal "${_ng_imgui_metal}")
string(REPLACE "texture2d<half, access::sample> texture [[texture(0)]])"
    "texture2d<half, access::sample> texture [[texture(0)]], constant uint &premultiplied [[buffer(2)]])" _ng_imgui_metal "${_ng_imgui_metal}")
string(REPLACE "    return half4(in.color) * texColor;"
    "    half4 result = half4(in.color) * texColor; if (!premultiplied) result.rgb *= texColor.a; result.rgb *= half(in.color.a); return result;" _ng_imgui_metal "${_ng_imgui_metal}")
string(REPLACE "pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;"
    "pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;" _ng_imgui_metal "${_ng_imgui_metal}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_metal_editor.mm" "${_ng_imgui_metal}")
add_library(nijigenerate_metal STATIC
    "${_ng_metal_sources}/context.mm" "${_ng_metal_sources}/backend.mm"
    "${_ng_metal_sources}/emitter.cpp" "${_ng_metal_sources}/compute.mm"
    "${_ng_metal_sources}/difference.mm" "${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_metal_editor.mm")
target_include_directories(nijigenerate_metal PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
    "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}" "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(nijigenerate_metal PUBLIC nicxlive::nicxlive nijigenerate_native_options
    nijigenerate_imgui SDL3::SDL3 "-framework Metal" "-framework QuartzCore" "-framework Cocoa")
target_compile_options(nijigenerate_metal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(nijigenerate_depth_gpu_render PUBLIC nijigenerate_metal)
target_link_libraries(nijigenerate_depth_draw_gpu PUBLIC nijigenerate_metal)
# A reused build directory may still contain the Vulkan bundle's generated
# runtime files. Metal must not ship an unused loader or ICD manifest.
add_custom_command(TARGET nijigenerate POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -f
        "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Frameworks/libvulkan.1.dylib"
        "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Frameworks/libMoltenVK.dylib"
        "$<TARGET_BUNDLE_DIR:nijigenerate>/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json"
    VERBATIM)
