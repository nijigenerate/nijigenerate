#include "output.hpp"
#include "output_internal.hpp"
#include <nijigenerate/compat/detail/plain_array_remove.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/selector/resource_proxy.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <imgui.h>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace nijigenerate::widgets::output {
namespace {
using core::selector::ResourcePtr;
using core::selector::ResourceType;
using Parameter = nicxlive::core::param::Parameter;
using Node = nicxlive::core::nodes::Node;

} // namespace

void ViewOutput::onUpdate() {
    if (project::incEditMode() == project::EditMode::VertexEdit) return;
    auto owner=weak_from_this().lock();
    detail::UuidMap<bool> contentsDrawn;
    compat::Slice<std::uint64_t> removed;
    const auto iteration = nodes;
    for (std::size_t index = 0; index < iteration.size(); ++index) {
        auto resource = compat::ngRequireObject(iteration[index]);
        const auto showContents = [&](ResourcePtr current) {
            if (contentsDrawn.contains(current->uuid())) return;
            const ImVec2 size{20, 20};
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
            if (ngButtonColored("\xEE\x97\x8D", size)) removed.append(static_cast<std::uint64_t>(index));

            if (current->type() == ResourceType::Node) {
                ImGui::SameLine();
                ImGui::Text(detail::outputCString(current->name().view()).data());
                auto node = core::selector::to<Node>(current);
                detail::onNodeView(node);
            } else if (current->type() == ResourceType::Parameter) {
                auto parameter = core::selector::to<Parameter>(current);
                ImGui::SameLine();
                auto puppet = project::incActivePuppet();
                panels::parameters::incParameterViewEditButtons<false, true>(current->index, parameter,
                    compat::ngRequireObject(puppet)->parameters, true);
                ImGui::SameLine();
                ImGui::Text(detail::outputCString(current->name().view()).data());
                detail::onParameterView(current->index, parameter);
            }
            ImGui::PopStyleVar(3);
            contentsDrawn[current->uuid()] = true;
        };

        char popupName[12];
        std::snprintf(popupName, sizeof(popupName), "###%x", static_cast<unsigned>(resource->uuid()));
        const auto alpha=core::selector::ngSelectorResourceObjectEquals(focused,resource) ? .8f : .1f;
        const auto textAlpha=core::selector::ngSelectorResourceObjectEquals(focused,resource) ? .8f : .3f;
        detail::setTransparency(alpha,textAlpha);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;
        if (resource->type() == ResourceType::Parameter) flags |= ImGuiWindowFlags_NoResize;
        if (ImGui::Begin(popupName, nullptr, flags)) {
            const auto cursor = ImGui::GetMousePos();
            const auto position = ImGui::GetWindowPos();
            const auto nan = std::numeric_limits<float>::quiet_NaN();
            ImVec2 size{nan, nan};
            if (resource->type() == ResourceType::Node) {
                size.x = detail::NodeViewWidth + 20;
                ImGui::SetWindowSize(size);
            } else if (resource->type() == ResourceType::Parameter) {
                auto proxy = std::dynamic_pointer_cast<core::selector::Proxy<Parameter>>(resource);
                auto parameter = compat::ngRequireObject(proxy)->obj();
                size = {166, compat::ngRequireObject(parameter)->isVec2 ? 186.f : 92.f};
                ImGui::SetWindowSize(size);
            }
            size = ImGui::GetWindowSize();
            const ImVec2 end{position.x + size.x, position.y + size.y};
            const bool inside = cursor.x >= position.x && cursor.x <= end.x &&
                cursor.y >= position.y && cursor.y <= end.y;
            if (inside) focused = resource;
            else if (core::selector::ngSelectorResourceObjectEquals(focused,resource)) focused.reset();
            showContents(resource);
            // Preserve the original's conditional End rather than adding a new false branch.
            ImGui::End();
        }
        ImGui::PopStyleColor(6);
    }
    for (auto count = removed.size(); count > 0; --count)
        nodes = compat::detail::ngRemovePlainArrayStable(nodes, static_cast<std::size_t>(removed[count - 1]));
}

} // namespace nijigenerate::widgets::output
