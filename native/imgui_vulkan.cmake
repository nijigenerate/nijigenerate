# Adapt only the renderer pipeline choice; keep the pinned upstream source intact.
file(READ "${NIJIGENERATE_IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp" _ng_imgui_vk)
string(REPLACE "    VkPipeline                  PipelineForViewports;"
    "    VkPipeline                  PremultipliedPipeline;\n    VkPipeline                  PremultipliedViewportPipeline;\n    VkPipeline                  PipelineForViewports;" _ng_imgui_vk "${_ng_imgui_vk}")
string(REPLACE "void ImGui_ImplVulkan_RenderDrawData(ImDrawData*"
    "static thread_local ImTextureID ngPremultipliedTexture = 0;\nvoid ngImGuiVulkanPremultipliedTexture(ImTextureID texture) { ngPremultipliedTexture = texture; }\n\nvoid ImGui_ImplVulkan_RenderDrawData(ImDrawData*" _ng_imgui_vk "${_ng_imgui_vk}")
string(REPLACE "VkPipeline* pipeline, uint32_t subpass)"
    "VkPipeline* pipeline, uint32_t subpass, bool premultiplied = false)" _ng_imgui_vk "${_ng_imgui_vk}")
string(REPLACE "color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;"
    "color_attachment[0].srcColorBlendFactor = premultiplied ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;" _ng_imgui_vk "${_ng_imgui_vk}")
set(_ng_vk_main_create "ImGui_ImplVulkan_CreatePipeline(v->Device, v->Allocator, v->PipelineCache, v->RenderPass, v->MSAASamples, &bd->Pipeline, v->Subpass);")
string(REPLACE "${_ng_vk_main_create}"
    "${_ng_vk_main_create}\n    ImGui_ImplVulkan_CreatePipeline(v->Device, v->Allocator, v->PipelineCache, v->RenderPass, v->MSAASamples, &bd->PremultipliedPipeline, v->Subpass, true);" _ng_imgui_vk "${_ng_imgui_vk}")
set(_ng_vk_viewport_create "ImGui_ImplVulkan_CreatePipeline(v->Device, v->Allocator, VK_NULL_HANDLE, wd->RenderPass, VK_SAMPLE_COUNT_1_BIT, &bd->PipelineForViewports, 0);")
string(REPLACE "${_ng_vk_viewport_create}"
    "${_ng_vk_viewport_create}\n    if (!bd->PremultipliedViewportPipeline) ImGui_ImplVulkan_CreatePipeline(v->Device, v->Allocator, VK_NULL_HANDLE, wd->RenderPass, VK_SAMPLE_COUNT_1_BIT, &bd->PremultipliedViewportPipeline, 0, true);" _ng_imgui_vk "${_ng_imgui_vk}")
string(REPLACE "                // Bind DescriptorSet with font or user texture"
    "                const auto selected_pipeline = pcmd->GetTexID() == ngPremultipliedTexture\n                    ? (pipeline == bd->PipelineForViewports ? bd->PremultipliedViewportPipeline : bd->PremultipliedPipeline) : pipeline;\n                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, selected_pipeline);\n                // Bind DescriptorSet with font or user texture" _ng_imgui_vk "${_ng_imgui_vk}")
string(REPLACE "    if (bd->PipelineLayout)       {"
    "    if (bd->PremultipliedPipeline) { vkDestroyPipeline(v->Device, bd->PremultipliedPipeline, v->Allocator); bd->PremultipliedPipeline = VK_NULL_HANDLE; }\n    if (bd->PremultipliedViewportPipeline) { vkDestroyPipeline(v->Device, bd->PremultipliedViewportPipeline, v->Allocator); bd->PremultipliedViewportPipeline = VK_NULL_HANDLE; }\n    if (bd->PipelineLayout)       {" _ng_imgui_vk "${_ng_imgui_vk}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/imgui_impl_vulkan_editor.cpp" "${_ng_imgui_vk}")
