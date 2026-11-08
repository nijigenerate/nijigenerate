#include "renderer.hpp"
#include "vulkan/backend.hpp"
#include "vulkan/compute.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/platform/imgui_sdl3_adapter.hpp>
#include <imgui_impl_sdl3.h>
#include <runtime_state.hpp>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <limits>
void ngImGuiVulkanPremultipliedTexture(ImTextureID);

namespace nijigenerate::backend {
namespace {
std::unique_ptr<vulkan::Context> owner;
std::shared_ptr<vulkan::Backend> runtime;
class DebugProvider final : public core::dbg::DebugProvider {
public:
    void initialize() override { runtime->initDebugRenderer(); }
    void pointSize(float n) override { runtime->setDebugPointSize(n); }
    void lineWidth(float n) override { runtime->setDebugLineWidth(n); }
    void upload(const nicxlive::core::math::Vec3Array& points,std::span<const uint16_t> indices) override {
        std::vector<nicxlive::core::math::Vec3> values(points.size());
        for(size_t i=0;i<points.size();++i) values[i]=points.at(i);
        runtime->uploadDebugBuffer(values,std::vector<uint16_t>(indices.begin(),indices.end()));
    }
    void external(nicxlive::core::RenderResourceHandle,nicxlive::core::RenderResourceHandle,int) override {
        throw std::logic_error("External debug buffers must be supplied as retained vertex arrays");
    }
    void points(nicxlive::core::math::Vec4 c,const nicxlive::core::math::Mat4& m) override { runtime->drawDebugPoints(c,m); }
    void lines(nicxlive::core::math::Vec4 c,const nicxlive::core::math::Mat4& m) override { runtime->drawDebugLines(c,m); }
};
}
void ngInitializeRenderer(SDL_Window* window) {
    owner=std::make_unique<vulkan::Context>(window);
    runtime=std::make_shared<vulkan::Backend>();
    nicxlive::core::inSetRenderBackend(runtime);
    core::dbg::ngInstallDebugProvider(std::make_shared<DebugProvider>(),runtime);
}
void ngInitializeRendererUI() {
    owner->initializeImGui();
    core::dbg::inInitDebug();
}
void ngBeginRendererFrame() {
    static const bool legacyIdle = [] {
        const char* value = std::getenv("NIJIGENERATE_VULKAN_LEGACY_IDLE");
        return value && std::strcmp(value, "1") == 0;
    }();
    // Detached ImGui windows can submit work outside the main frame fence.
    if (legacyIdle || (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        owner->idle();
    else {
        static const bool earlyFence = [] {
            const char* value = std::getenv("NIJIGENERATE_VULKAN_EARLY_FENCE");
            return value && std::strcmp(value, "1") == 0;
        }();
        // CPU scene preparation can overlap the previous submission.
        // Context::beginFrame waits before the shared GPU buffers are reused.
        if (earlyFence) owner->waitForPreviousFrame();
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}
void ngCreateRendererFonts() { ImGui_ImplVulkan_CreateFontsTexture(); }
void ngDestroyRendererFonts() { ImGui_ImplVulkan_DestroyFontsTexture(); }
bool ngRendererProcessEvent(const SDL_Event* event) { return ImGui_ImplSDL3_ProcessEvent(event); }
void ngRenderUI(ImDrawData* data) {
    if(!owner->beginFrame()) return;
    runtime->render();
    ngImGuiVulkanPremultipliedTexture(runtime->uiTexture(uint32_t(runtime->renderImageHandle())));
    // Application texture IDs stay opaque 32-bit resource keys. Only the ImGui
    // submission translates them to device-owned Vulkan descriptor sets.
    std::vector<std::pair<ImDrawCmd*,ImTextureID>> translated;
    for(auto* list:data->CmdLists) for(auto& cmd:list->CmdBuffer) {
        auto id=cmd.GetTexID();
        if(!cmd.UserCallback && id<=std::numeric_limits<uint32_t>::max()) {
            translated.emplace_back(&cmd,id);
            cmd.TextureId=runtime->uiTexture(runtime->hasTexture(uint32_t(id))?uint32_t(id):0);
        }
    }
    try { owner->present(data); }
    catch(...) { for(auto [cmd,id]:translated) cmd->TextureId=id; throw; }
    for(auto [cmd,id]:translated) cmd->TextureId=id;
    if(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows(); ImGui::RenderPlatformWindowsDefault();
    }
}
void ngShutdownRenderer() {
    if(!owner) return;
    owner->idle(); vulkan::ngShutdownComputeJobs(); core::dbg::ngClearDebugProvider();
    runtime->shutdown(); nicxlive::core::inSetRenderBackend({}); runtime.reset();
    owner->shutdownImGui(); owner.reset();
}
}
