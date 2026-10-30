#pragma once
#include "command_browser.hpp"
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <bit>

namespace nijigenerate::windows::command_browser {
namespace detail {
inline std::string browserCString(BrowserText value) {
#ifndef NDEBUG
    auto contents = value.view();
    while (!contents.empty() && contents.back() == '\0') contents.remove_suffix(1);
    if (contents.find('\0') != std::string_view::npos)
        throw compat::AssertError{"toStringz input contains an embedded zero", __FILE__, __LINE__};
#endif
    return std::string(value.view());
}
template<class T> BrowserText resourceSelectionIds(compat::Slice<T> selected) {
    compat::Slice<BrowserText> ids;
    for (auto item : selected) {
        if (!item) continue;
        if constexpr (std::same_as<T, NodeRef> || std::same_as<T, ParameterRef>) {
            const auto id = item->getUUID();
            ids.append(browserDecimal(id));
        } else if constexpr (std::same_as<T, BindingRef>) {
            const auto id = ensureCommandBrowserBindingId(item);
            ids.append(browserDecimal(id));
        }
    }
    return joinBrowserTexts(ids, ",");
}
}
template<class T> bool renderResourcePicker(BrowserText id, compat::Slice<T>& selected, PuppetRef puppet,
    bool (*acceptsNode)(NodeRef) = nullptr) {
    bool changed = false;
    if (selected.isNull()) selected = compat::Slice<T>();
    ImGui::PushID(detail::sourceCString(id).data());
    if (selected.empty()) {
        if (widgets::ngButtonColored("\356\205\205", ImVec2(24,24))) {
            selected.append(T{}); changed = true;
        }
        ImGui::PopID(); return changed;
    }
    std::size_t i = 0;
    while (i < selected.size()) {
        auto sel = compat::ngArrayAt(selected, i);
        ImGui::PushID(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i)));
        auto label = labelFor(sel);
        if (ImGui::BeginCombo("##combo", detail::sourceCString(label).data())) {
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetStyle().IndentSpacing * 0.25f);
            auto chosen = sel;
            if constexpr (std::same_as<T, NodeRef>) {
                if (puppet && puppet->root) renderNodeTree(puppet->root, chosen, acceptsNode);
            } else if constexpr (std::same_as<T, ParameterRef>) renderParameterTree(puppet, chosen);
            else if constexpr (std::same_as<T, BindingRef>) renderBindingTree(puppet, chosen);
            ImGui::EndCombo(); ImGui::PopStyleVar(1);
            if (chosen != sel) {
                {
                    auto selectedOwner = selected;
                    compat::ngArrayAt(selectedOwner, i) = chosen;
                }
                changed = true;
            }
        }
        ImGui::SameLine(0,0);
        if (widgets::ngButtonColored("\356\244\256", ImVec2(24,24))) {
            const auto before = compat::ngArraySlice(selected, 0, i);
            const auto after = compat::ngArraySlice(selected, i + 1, selected.size());
            compat::Slice<T> next;
            next.append(before);
            next.append(after);
            selected = next; changed = true; ImGui::PopID(); continue;
        }
        ImGui::PopID(); ++i;
    }
    ImGui::SameLine(0,0);
    if (widgets::ngButtonColored("\356\205\205", ImVec2(24,24))) { selected.append(T{}); changed = true; }
    ImGui::PopID(); return changed;
}
}
