#include "project.hpp"
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/windows/base.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <timing.hpp>
#include <bit>
#include <cstdint>

namespace nijigenerate::project {
compat::Slice<compat::RetainedString> incAnimationKeysGet() {
    auto animations = compat::ngRequireObject(incActivePuppet())->getAnimations();
    compat::Slice<compat::RetainedString> result;
    for (auto entry = animations.begin(); entry != animations.end(); ++entry) {
        result.append(entry->first);
    }
    return result;
}
void incAnimationRender() {
    const auto player = incAnimationPlayerGet();
    compat::ngRequireObject(player)->update(static_cast<float>(nicxlive::core::deltaTime()));
    if (auto current = incAnimationGet()) current->render();
}
namespace {
int nextFrame(int value) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1u); }
void changeFrames(bool remove) {
    using namespace core::actionstack;
    incActionPushGroup();
    compat::ngRunWithScopeExit([&] {
        // D foreach captures the source slice before callbacks replace animation.lanes.
        auto lanes = compat::ngRequireObject(compat::ngRequireObject(incAnimationGet())->animation())->lanes;
        for (auto& lane : lanes) {
            auto parameter = compat::ngRequireObject(lane.paramRef)->targetParam;
            auto axis = compat::ngRequireObject(lane.paramRef)->targetAxis;
            if (remove) incAnimationKeyframeRemove(parameter, axis);
            else {
                const auto value = compat::ngRequireObject(parameter)->value;
                if (axis < 0 || axis >= 2)
                    throw compat::ngStaticError<compat::ArrayIndexError>(static_cast<std::size_t>(axis), 2,
                        compat::RetainedString::fromStaticLiteral("source/nijigenerate/project.d"), 1150);
                incAnimationKeyframeAdd(parameter, axis, axis == 0 ? value.x : value.y);
            }
        }
    }, [] { incActionPopGroup(); });
}
}
void incAnimationUpdate() {
    if (incAnimationGet() && (compat::ngRequireObject(incAnimationGet())->isRunning() ||
            compat::ngRequireObject(incAnimationGet())->isPlayingLeadOut()) &&
            !compat::ngRequireObject(incAnimationGet())->paused()) incAnimationRender();
    if (incEditMode() != EditMode::AnimEdit || !incAnimationGet() || windows::base::incGetWindowsOpen() != 0) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        if (!compat::ngRequireObject(incAnimationGet())->playing() ||
                compat::ngRequireObject(incAnimationGet())->paused()) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                    (ImGui::IsKeyDown(ImGuiKey_RightCtrl) &&
                        compat::ngRequireObject(incAnimationGet())->frame() != 0))
                compat::ngRequireObject(incAnimationGet())->seek(0);
            compat::ngRequireObject(incAnimationGet())->play(true);
        } else {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
                compat::ngRequireObject(incAnimationPlayerGet())->stopAll(
                    ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift));
            else compat::ngRequireObject(incAnimationGet())->pause();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) &&
            compat::ngRequireObject(incAnimationGet())->frame() > 0) {
        compat::ngRequireObject(incAnimationGet())->seek(
            compat::ngRequireObject(incAnimationGet())->frame() - 1); incAnimationRender();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
        const auto next = nextFrame(compat::ngRequireObject(incAnimationGet())->frame());
        if (next < compat::ngRequireObject(incAnimationGet())->frames()) {
            compat::ngRequireObject(incAnimationGet())->seek(
                nextFrame(compat::ngRequireObject(incAnimationGet())->frame())); incAnimationRender();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_N, false) || ImGui::IsKeyPressed(ImGuiKey_Insert, false)) changeFrames(false);
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false)) changeFrames(true);
}
namespace {
void selectAllRecurse(std::shared_ptr<Node> node) {
    incAddSelectNode(node);
    const auto children = compat::ngRequireObject(node)->children;
    for (const auto child : children) selectAllRecurse(child);
}
}
void incSelectAll() {
    if (incArmedParameter()) return;
    incSelectNode();
    const auto children = compat::ngRequireObject(
        compat::ngRequireObject(incActivePuppet())->root)->children;
    for (const auto child : children) selectAllRecurse(child);
}
}
