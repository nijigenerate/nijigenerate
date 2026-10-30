// Copyright 2020-2023 Inochi2D Project, 2024 nijigenerate Project.
// Author: Luna Nielsen. Distributed under the 2-Clause BSD License.
#include "animlist.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/windows/editanim.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <imgui.h>

namespace nijigenerate::panels::animlist {
using compat::RetainedString;
using windows::editanim::EditAnimationWindow;

AnimListPanel::AnimListPanel()
    : Panel(RetainedString::fromStaticLiteral("Animation List"),
        core::translation::ngTranslateRetained(RetainedString::fromStaticLiteral("Animation List")), false) {
    activeModes = project::EditMode::AnimEdit;
}

void AnimListPanel::onUpdate() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto current = project::incAnimationGet();
    const auto currentName = [&] {
        if (!current) return RetainedString::emptyLiteral();
        return current->name();
    }();

    if (ImGui::BeginChild("ANIM_LIST", ImVec2(0, -32), ImGuiChildFlags_Borders)) {
        const auto puppet = project::incActivePuppet();
        auto animations = compat::ngRequireObject(puppet)->getAnimations();
        for (auto entry = animations.begin(); entry != animations.end(); ++entry) {
            const auto animation = animations.retain(entry);
            const auto retainedName = entry->first;
            const char* keyBytes = retainedName.data();
            const char* keyEnd = keyBytes ? keyBytes + retainedName.size() : nullptr;
            ImGui::PushID(keyBytes, keyEnd);
            if (ImGui::BeginPopup("###OPTIONS")) {
                const auto editLabel = core::translation::ngTranslateCString("Edit...");
                if (ImGui::MenuItem(editLabel.data())) {
                    windows::base::incPushWindow(windows::base::ngCreateWindow<EditAnimationWindow>(
                        *animation, retainedName));
                }
                const auto deleteLabel = core::translation::ngTranslateCString("Delete");
                if (ImGui::MenuItem(deleteLabel.data())) {
                    project::ngAnimationDelete(retainedName);
                    ImGui::EndPopup();
                    ImGui::PopID();
                    ImGui::EndChild();
                    return;
                }
                ImGui::EndPopup();
            }

            const compat::SourceCString selectableName(retainedName);
            compat::detail::ngSourceCStringPostconditions(retainedName.view(), selectableName.data());
            if (ImGui::Selectable(selectableName.data(), retainedName == currentName)) {
                project::incAnimationChange(retainedName);
            }
            ImGui::OpenPopupOnItemClick("###OPTIONS", ImGuiPopupFlags_MouseButtonRight);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    widgets::ngDummy(ImVec2(0, 2));
    if (ImGui::BeginChild("ANIM_BTNS", ImVec2(0, 0), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        widgets::ngDummy(ImVec2(-26, 0));
        ImGui::SameLine(0, 0);
        if (widgets::ngButtonColored("\xEE\x85\x85", ImVec2(24, 24))) {
            windows::base::incPushWindow(windows::base::ngCreateWindow<EditAnimationWindow>());
        }
    }
    ImGui::EndChild();
}

void ngInitializeAnimListPanelModule() {
    ngInitializePanelModule<AnimListPanel>();
}

} // namespace nijigenerate::panels::animlist
