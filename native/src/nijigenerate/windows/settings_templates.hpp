#pragma once
#include "settings.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/commands/all_maps.hpp>
#include <nijigenerate/commands/detail/map_selection.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/shortcut/persistence.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <vector>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::windows::settings {
namespace detail {
template<class Operation> decltype(auto) allocateSettingsMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::SourceCString settingsCString(std::string_view input) {
    compat::SourceCString result(input);
    compat::detail::ngSourceCStringPostconditions(input, result.data());
    return result;
}
inline compat::RetainedString retainSettingsBuffer(const char* input) {
    const auto length = std::char_traits<char>::length(input);
    // Source fromStringz(...).idup returns null for an empty duplicated slice.
    if (!length) return {};
    return allocateSettingsMemory([&] { return compat::RetainedString(std::string_view(input, length)); });
}
template<std::size_t N> compat::RetainedStringSlice ngShortcutTr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
inline compat::RetainedString ngShortcutLower(compat::RetainedString text) { return io::depthmap_psd::ngPsdDepthLowerString(text); }
inline compat::RetainedString ngShortcutReplace(compat::RetainedString text,std::string_view before,std::string_view after) {
    if (before.empty()) return text;
    auto offset=text.view().find(before);
    if (offset==std::string_view::npos) return text;
    return allocateSettingsMemory([&] {
        std::string result(text.view());
        do {
            result.replace(offset,before.size(),after);
            offset+=after.size();
            offset=result.find(before,offset);
        } while (offset!=std::string::npos);
        return compat::RetainedString(std::move(result));
    });
}
} // namespace detail

template<class Table> bool SettingsWindow::hasMatchingCommands(Table& table) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    using namespace core::settings;
    using namespace core::shortcut;
    using namespace core::input;
    using namespace widgets;
    // Source foreach retains the current class reference across virtual calls.
    for (auto [key,command] : table) {
        if (!compat::ngRequireObject(command)->shortcutRunnable()) continue;
        if (shortcutFilter.empty()) return true;
        if (detail::ngShortcutLower(compat::ngRequireObject(command)->label()).view().find(detail::ngShortcutLower(shortcutFilter).view()) != std::string_view::npos) return true;
    }
    return false;
}
template<class Table> void SettingsWindow::renderCommandTable(Table& table,const char* title) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    using namespace core::settings;
    using namespace core::shortcut;
    using namespace core::input;
    using namespace widgets;
    using Key = commands::detail::KeyTypeOfAA<Table>;
    std::vector<Key, nicxlive::core::d_runtime::SourceAllocator<Key>> keys;
    for (auto [key,command] : table) {
        if (!compat::ngRequireObject(command)->shortcutRunnable()) continue;
        if (shortcutFilter.empty() ||
            detail::ngShortcutLower(compat::ngRequireObject(command)->label()).view().find(
                detail::ngShortcutLower(shortcutFilter).view()) != std::string_view::npos) {
            if (keys.size() == keys.max_size())
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            keys.push_back(key);
        }
    }
    if (keys.empty()) return;
    if (ngBeginCategory(title)) {
        constexpr auto tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
        if (ImGui::BeginTable(title,3,tableFlags,ImVec2(0,0),0)) {
            ImGui::TableSetupColumn(core::translation::ngTranslateCString("Action").data(),ImGuiTableColumnFlags_None,.6,0);
            ImGui::TableSetupColumn(core::translation::ngTranslateCString("Shortcut").data(),ImGuiTableColumnFlags_None,.2,1);
            ImGui::TableSetupColumn(core::translation::ngTranslateCString("Edit").data(),ImGuiTableColumnFlags_None,.2,2);
            ImGui::TableHeadersRow();
            for (const auto& key : keys) {
                auto found = table.find(key);
                std::shared_ptr<commands::Command> command;
                if constexpr (std::is_pointer_v<decltype(found)>) {
                    if (!found) continue;
                    command = *found;
                } else {
                    if (found == table.end()) continue;
                    command = found->second;
                }
                auto label = compat::ngRequireObject(command)->label();
                ImGui::TableNextRow(ImGuiTableRowFlags_None,0); ImGui::TableSetColumnIndex(0);
                ngText(label); ImGui::TableSetColumnIndex(1);
                auto shortcut = ngShortcutFor(command);
#if defined(__APPLE__)
                if (incSettingsGet<bool>(SettingsString(compat::RetainedString::fromStaticLiteral("MacSwapCmdCtrl")),false) && !shortcut.empty()) {
                    const auto controlLabel = ngModifierLabelCtrl();
                    const auto superLabel = ngModifierLabelSuper();
                    shortcut=detail::ngShortcutReplace(shortcut,controlLabel,"__CTRL_TMP__");
                    shortcut=detail::ngShortcutReplace(shortcut,superLabel,controlLabel);
                    shortcut=detail::ngShortcutReplace(shortcut,"__CTRL_TMP__",superLabel);
                }
#endif
                if (capturingShortcut && capturingCommand == command) {
                    if (capturingPreview.empty()) ngTextColored(ImVec4(.9,.7,.2,1),detail::ngShortcutTr("Press keys..."));
                    else ngTextColored(ImVec4(.9,.7,.2,1),capturingPreview);
                } else {
                    if (shortcut.empty()) ngText(detail::ngShortcutTr("<none>"));
                    else ngText(shortcut);
                }
                ImGui::TableSetColumnIndex(2);
                const auto setLabel = detail::settingsCString("\ue145");
                auto id = commands::ngCommandKeyName(key);
                ImGui::PushID(detail::settingsCString(std::string_view(id)).data());
                if (ngButtonColored(setLabel.data(),ImVec2(0,0))) {
                    capturingShortcut = true; capturingCommand = command;
                    capturingPreview = compat::RetainedString::emptyLiteral(); capturingRepeat = false;
                    ngSetShortcutCapture(true);
                }
                ImGui::SameLine(0,4);
                const auto clearLabel = detail::settingsCString("\ue5cd");
                if (ngButtonColored(clearLabel.data(),ImVec2(0,0))) { ngClearShortcutFor(command); ngSaveShortcutsToSettings(); }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ngEndCategory();
}
} // namespace nijigenerate::windows::settings
