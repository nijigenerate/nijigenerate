#include "settings.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
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
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <vector>
namespace nijigenerate::windows::settings {
using namespace core::settings;
using namespace core::shortcut;
using namespace core::input;
using namespace widgets;
void SettingsWindow::captureShortcutUI() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!capturingShortcut) return;
    auto& input = ImGui::GetIO();
    ImGui::Separator(); ngTextColored(ImVec4(.9,.7,.2,1),detail::ngShortcutTr("Capturing shortcut..."));
    ImGui::SameLine(0,8); ngCheckbox(core::translation::ngTranslateCString("Repeat").data(),&capturingRepeat); ImGui::SameLine(0,8);
    if (ngButtonColored(core::translation::ngTranslateCString("Cancel").data(),ImVec2(0,0))) {
        capturingShortcut = false; capturingCommand.reset(); capturingPreview = compat::RetainedString::emptyLiteral();
        ngSetShortcutCapture(false);
    }
    static constexpr std::string_view keys[]{"A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z","0","1","2","3","4","5","6","7","8","9",
        "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Left","Right","Up","Down"};
    std::string_view pressed;
    for (auto key : keys) {
        auto code = ngKeyScancode(key);
        if (code != ImGuiKey_None && ImGui::IsKeyPressed(code,false)) { pressed = key; break; }
    }
    auto preview = ngFormatShortcut(input.KeyCtrl,input.KeyAlt,input.KeyShift,input.KeySuper,pressed);
    capturingPreview = preview;
    if (!pressed.empty()) {
        if (capturingCommand) {
            ngClearShortcutFor(capturingCommand);
            ngRegisterShortcut(preview,capturingCommand,capturingRepeat); ngSaveShortcutsToSettings();
        }
        capturingShortcut = false; capturingCommand.reset(); capturingPreview = compat::RetainedString::emptyLiteral();
        ngSetShortcutCapture(false);
    }
    ImGui::Separator();
}
void SettingsWindow::drawShortcutsPane() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    beginSection(core::translation::ngTranslateCString("Shortcuts").data());
    ngText(detail::ngShortcutTr("Assign shortcuts to commands. Click Set and press keys. Use Clear to remove a binding."));
#if defined(__APPLE__)
    const SettingsString swapKey(compat::RetainedString::fromStaticLiteral("MacSwapCmdCtrl"));
    bool swap = incSettingsGet<bool>(swapKey,false);
    if (ngCheckbox(core::translation::ngTranslateCString("Swap Command and Control").data(),&swap)) incSettingsSet(swapKey,swap);
    ngTooltip(detail::ngShortcutTr("When enabled, Command (\ueae7) and Control (\ueae6) are treated as swapped for shortcuts."));
    ngDummy(ImVec2(0,4));
#endif
    endSection(); captureShortcutUI();
    if (ImGui::BeginChild("ShortcutsTables",ImVec2(0,0),true)) {
        ngText(detail::ngShortcutTr("Filter actions by name:"));
        if (ImGui::InputText("##ShortcutFilter",shortcutFilterBuffer.data(),shortcutFilterBuffer.size(),ImGuiInputTextFlags_None))
            shortcutFilter = detail::retainSettingsBuffer(shortcutFilterBuffer.data());
        renderCommandTable(commands::puppet::file::commands,core::translation::ngTranslateCString("File").data());
        renderCommandTable(commands::puppet::edit::commands,core::translation::ngTranslateCString("Edit").data());
        renderCommandTable(commands::puppet::view::commands,core::translation::ngTranslateCString("View").data());
        renderCommandTable(commands::view::panel::togglePanelCommands,core::translation::ngTranslateCString("View/Panels").data());
        renderCommandTable(commands::puppet::tool::commands,core::translation::ngTranslateCString("Tools").data());
        renderCommandTable(commands::viewport::control::commands,core::translation::ngTranslateCString("Viewport").data());
        renderCommandTable(commands::viewport::palette::commands,core::translation::ngTranslateCString("Palette").data());
        renderCommandTable(commands::mesheditor::tool::selectToolModeCommands,core::translation::ngTranslateCString("Mesh Editor Tools").data());
        if (hasMatchingCommands(commands::automesh::dynamic::autoMeshApplyCommands))
            renderCommandTable(commands::automesh::dynamic::autoMeshApplyCommands,core::translation::ngTranslateCString("AutoMesh").data());
        bool hasNodes = hasMatchingCommands(commands::node::node::commands) ||
            hasMatchingCommands(commands::node::dynamic::addNodeCommands) ||
            hasMatchingCommands(commands::node::dynamic::insertNodeCommands) ||
            hasMatchingCommands(commands::node::dynamic::convertNodeCommands);
        if (hasNodes && ngBeginCategory(core::translation::ngTranslateCString("Nodes").data())) {
            renderCommandTable(commands::node::node::commands,core::translation::ngTranslateCString("Nodes").data());
            renderCommandTable(commands::node::dynamic::addNodeCommands,core::translation::ngTranslateCString("Add Node").data());
            renderCommandTable(commands::node::dynamic::insertNodeCommands,core::translation::ngTranslateCString("Insert Node").data());
            renderCommandTable(commands::node::dynamic::convertNodeCommands,core::translation::ngTranslateCString("Convert Node").data());
            ngEndCategory();
        }
        renderCommandTable(commands::inspector::apply_node::commands,core::translation::ngTranslateCString("Panel/Inspector").data());
        renderCommandTable(commands::parameter::param::commands,core::translation::ngTranslateCString("Parameter").data());
        renderCommandTable(commands::parameter::paramedit::commands,core::translation::ngTranslateCString("Parameter Edit").data());
        renderCommandTable(commands::binding::commands,core::translation::ngTranslateCString("Binding").data());
        renderCommandTable(commands::parameter::group::commands,core::translation::ngTranslateCString("Parameter Group").data());
        renderCommandTable(commands::parameter::animedit::commands,core::translation::ngTranslateCString("Animation Edit").data());
    }
    ImGui::EndChild();
}
}
