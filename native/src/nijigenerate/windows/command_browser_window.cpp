#include "command_browser_helpers.hpp"
#include "command_browser_sort.hpp"
#include <nijigenerate/commands/viewport/palette.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
namespace nijigenerate::windows::command_browser {
using namespace detail;
using namespace commands;
void CommandBrowserWindow::onUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!contextDirty) captureDefaultContext();
    auto available = widgets::ngAvailableSpace();
    ImGui::PushItemWidth(-1);
    const auto searchText = core::translation::ngTranslateCString("Search");
    const auto searchView = std::string_view(searchText.data());
    const auto searchLabel = searchView.empty() ? BrowserText{} : text(searchView);
    input(staticText("COMMAND_BROWSER_SEARCH"),searchLabel,filterText);
    ImGui::PopItemWidth(); ImGui::SameLine();
    if (ImGui::Button(core::translation::ngTranslateCString("Refresh").data())) rebuildCommandInfos();
    compat::Slice<CommandRef> filteredCommands;
    for (auto command : commands::viewport::palette::filterCommands(filterText.retainedString()))
        if (ngCommandAllowedInCurrentContext(command)) filteredCommands.append(command);
    compat::Slice<CommandInfo> filtered;
    for (auto command : filteredCommands) {
        auto found = gCommandInfosByCmd.find(command);
        if (found != gCommandInfosByCmd.end()) filtered.append(found->second);
    }
    row_sort::sort({filtered.data(), filtered.size()});
    if (selectedCmd) {
        for (std::size_t i = 0; i < filtered.size(); ++i)
            if (filtered[i].cmd == selectedCmd) { selectedIndex = i; break; }
    }
    if (selectedIndex >= filtered.size()) selectedIndex = filtered.empty() ? 0 : filtered.size() - 1;
    if (!filtered.empty()) {
        if (!selectedCmd || compat::ngArrayAt(filtered, selectedIndex).cmd != selectedCmd)
            resetSelection(compat::ngArrayAt(filtered, selectedIndex));
    } else selectedCmd = {};
    float listWidth = available.x * 0.35f;
    if (ImGui::BeginChild("command_list",ImVec2(listWidth,0),true)) {
        for (std::size_t i = 0; i < filtered.size(); ++i) {
            auto info = filtered[i];
            bool selected = info.cmd == selectedCmd || (!selectedCmd && i == selectedIndex);
            if (ImGui::Selectable(sourceCString(format("%s##cmd_%s",
                    compat::ngRequireObject(info.cmd)->label(),info.id)).data(),selected)) {
                if (selectedCmd != info.cmd) resetSelection(info);
                selectedIndex = i;
            }
            if (ImGui::IsItemHovered()) widgets::ngTooltip(info.id);
            ImGui::TextDisabled(sourceCString(info.id).data());
        }
    }
    ImGui::EndChild(); ImGui::SameLine();
    if (ImGui::BeginChild("command_detail",ImVec2(0,0),true)) {
        if (filtered.empty()) ImGui::Text(core::translation::ngTranslateCString("No commands match the filter.").data());
        else {
            auto info = compat::ngArrayAt(filtered, selectedIndex);
            ImGui::Text(sourceCString(format("%s",compat::ngRequireObject(info.cmd)->label())).data());
            ImGui::TextDisabled(sourceCString(info.id).data());
            if (!compat::ngRequireObject(info.cmd)->description().empty())
                ImGui::TextWrapped(sourceCString(
                    BrowserText(compat::ngRequireObject(info.cmd)->description())).data());
            ImGui::Separator();
            ImGui::Text(core::translation::ngTranslateCString("Inputs (arguments + optional context)").data());
            drawInputsTable(info);
            ImGui::Separator(); ImGui::Text(core::translation::ngTranslateCString("Run").data());
            if (widgets::ngButtonColored(core::translation::ngTranslateCString("Run Command").data())) executeSelected(info);
            ImGui::Separator(); ImGui::Text(core::translation::ngTranslateCString("Output (last run)").data());
            drawOutputsTable(info);
        }
    }
    ImGui::EndChild();
}
}
