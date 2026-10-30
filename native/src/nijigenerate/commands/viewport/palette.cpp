#include "palette_types.hpp"
#include "palette_command.hpp"
#include "detail/palette_identity.hpp"
#include "palette.hpp"
#include <nijigenerate/commands/concrete_catalog.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <imgui.h>
#include <algorithm>

#include <cstddef>
#include <cstdint>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::commands::viewport::palette {
namespace {
compat::RetainedString gPaletteQuery;
std::size_t gPaletteSelectedIndex = 0;
bool gPaletteActive = false;
bool gPaletteFocusPending = false;
std::uint64_t gPalettePopupId = 0;

void paletteOpen() {
    gPaletteQuery = compat::RetainedString::emptyLiteral();
    gPaletteSelectedIndex = 0;
    gPaletteActive = true;
    gPaletteFocusPending = true;
    gPalettePopupId = 0;
}
void paletteClose() {
    gPaletteActive = false;
    if (gPalettePopupId) widgets::NotificationPopup::instance()->close(gPalettePopupId);
}
compat::RetainedString lower(compat::RetainedStringSlice text) {
    return io::imageexport::detail::lowerExtension(text);
}
template<class Table> bool inTable(const Table& table, const std::shared_ptr<Command>& command) {
    for (const auto& [key, value] : table) if (value == command) return true;
    return false;
}
}
compat::Slice<std::shared_ptr<Command>> collectAllCommands() {
    compat::Slice<std::shared_ptr<Command>> result;
    ngForEachCommandMap([&](const auto& table) {
        for (const auto& [key, value] : table) if (value) result.append(value);
    });
    return result;
}
compat::RetainedString getParentCategory(std::shared_ptr<Command> command) {
#define NG_CATEGORY(Table, Label) if (inTable(Table, command)) return core::translation::ngTranslateRetained( \
    compat::RetainedString::fromStaticLiteral(Label));
    NG_CATEGORY(puppet::file::commands, "File")
    NG_CATEGORY(puppet::edit::commands, "Edit")
    NG_CATEGORY(puppet::view::commands, "View")
    NG_CATEGORY(view::panel::togglePanelCommands, "View/Panels")
    NG_CATEGORY(puppet::tool::commands, "Tools")
    NG_CATEGORY(viewport::control::commands, "Viewport")
    NG_CATEGORY(viewport::palette::commands, "Palette")
    NG_CATEGORY(mesheditor::tool::selectToolModeCommands, "Mesh Editor Tools")
    NG_CATEGORY(automesh::dynamic::autoMeshApplyCommands, "AutoMesh")
    NG_CATEGORY(automesh::config::autoMeshGetConfigCommands, "AutoMesh/Config")
    NG_CATEGORY(automesh::config::autoMeshSetConfigCommands, "AutoMesh/Config")
    NG_CATEGORY(automesh::config::autoMeshSetSimpleCommands, "AutoMesh/Set Simple")
    NG_CATEGORY(automesh::config::autoMeshSetAdvancedCommands, "AutoMesh/Set Advanced")
    NG_CATEGORY(automesh::config::autoMeshSetPresetCommands, "AutoMesh/Presets")
    NG_CATEGORY(automesh::config::autoMeshTypedCommands, "AutoMesh/Typed")
    NG_CATEGORY(node::node::commands, "Nodes")
    NG_CATEGORY(node::dynamic::addNodeCommands, "Add Node")
    NG_CATEGORY(node::dynamic::insertNodeCommands, "Insert Node")
    NG_CATEGORY(node::dynamic::convertNodeCommands, "Convert Node")
    NG_CATEGORY(inspector::apply_node::commands, "Panel/Inspector")
    NG_CATEGORY(parameter::param::commands, "Parameter")
    NG_CATEGORY(parameter::paramedit::commands, "Parameter Edit")
    NG_CATEGORY(binding::commands, "Binding")
    NG_CATEGORY(parameter::group::commands, "Parameter Group")
    NG_CATEGORY(parameter::animedit::commands, "Animation Edit")
#undef NG_CATEGORY
    return compat::RetainedString::emptyLiteral();
}
compat::RetainedString deriveEnglishToken(std::shared_ptr<Command> command) {
    auto name = ngCommandSourceClassName(command);
    const auto dot = name.view().find_last_of('.');
    if (dot != std::string_view::npos) name = name.subslice(dot + 1, name.size());
    constexpr std::string_view suffix = "Command";
    if (name.size() > suffix.size() && name.view().ends_with(suffix))
        name = name.subslice(0, name.size() - suffix.size());
    const auto upper = [](unsigned char c) { return c >= 'A' && c <= 'Z'; };
    const auto small = [](unsigned char c) { return c >= 'a' && c <= 'z'; };
    const auto alnum = [&](unsigned char c) { return upper(c) || small(c) || (c >= '0' && c <= '9'); };
    compat::RetainedString copied;
    try {
        std::string result;
        for (std::size_t i = 0; i < name.size(); ++i) {
            const auto ch = name.view()[i];
            if (i && ((upper(ch) && small(name.view()[i - 1])) || (alnum(ch) && !alnum(name.view()[i - 1]))))
                result += ' ';
            result += ch;
        }
        copied = result.empty() ? compat::RetainedString{}
            : compat::RetainedString(std::string_view(result));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return lower(copied);
}
compat::Slice<std::shared_ptr<Command>> filterCommands(compat::RetainedString query,
        std::shared_ptr<Command> exclude) {
    const auto q = lower(query);
    compat::Slice<std::shared_ptr<Command>> result;
    for (const auto& command : collectAllCommands()) {
        if (!command || !ngCommandAllowedInCurrentContext(command) || (exclude && command == exclude)) continue;
        const auto label = command->label();
        const auto english = deriveEnglishToken(command);
        const auto parentEn = getParentCategory(command);
        const auto parentLc = lower(parentEn);
        const auto parentLocLc = parentLc;
        if (q.empty() || lower(label).view().find(q.view()) != std::string_view::npos ||
            english.view().find(q.view()) != std::string_view::npos ||
            (!parentLc.empty() && parentLc.view().find(q.view()) != std::string_view::npos) ||
            (!parentLocLc.empty() && parentLocLc.view().find(q.view()) != std::string_view::npos))
            result.append(command);
    }
    return result;
}
ListCommandCommand::ListCommandCommand()
    : ListCommandCommand(SourceConstructorFrame::capture()) {}
ListCommandCommand::SourceConstructorFrame ListCommandCommand::SourceConstructorFrame::capture() {
    return {core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Show Command Palette."))};
}
ListCommandCommand::ListCommandCommand(SourceConstructorFrame frame)
    : ExCommand(std::move(frame.description)) {
    try {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ListCommandCommand),
            compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.palette.ListCommandCommand"));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void ListCommandCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ListCommandCommand>();
}
std::shared_ptr<CommandResult> ListCommandCommand::run(std::shared_ptr<Context>) {
    const auto self = shared_from_this();
    paletteOpen();
    gPalettePopupId = widgets::NotificationPopup::instance()->popup([self](ImGuiIO* io) {
        if (!gPaletteActive) return;
        const float halfWidth = io->DisplaySize.x * 0.5f;
        ImGui::PushItemWidth(halfWidth);
        if (gPaletteFocusPending) { ImGui::SetKeyboardFocusHere(); gPaletteFocusPending = false; }
        const bool submitted = widgets::ngInputText(compat::RetainedString::fromStaticLiteral("PALETTE_QUERY"),
            gPaletteQuery, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        const auto filtered = filterCommands(gPaletteQuery, self);
        if (gPaletteSelectedIndex >= filtered.size()) gPaletteSelectedIndex = filtered.empty() ? 0 : filtered.size() - 1;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) && !filtered.empty() && gPaletteSelectedIndex + 1 < filtered.size())
            ++gPaletteSelectedIndex;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && !filtered.empty() && gPaletteSelectedIndex > 0)
            --gPaletteSelectedIndex;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { paletteClose(); return; }
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float estimated = std::max<std::size_t>(filtered.size(), 4) * lineHeight;
        const float maximumHeight = io->DisplaySize.y * 0.8f;
        float height = estimated < maximumHeight ? estimated : maximumHeight;
        if (height < lineHeight * 3 && !filtered.empty()) height = lineHeight * filtered.size();
        if (ImGui::BeginChild("PALETTE_RESULTS", ImVec2(halfWidth, height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            for (std::size_t i = 0; i < filtered.size(); ++i) {
                const auto command = filtered[i];
                const auto label = command->label();
                const auto parent = getParentCategory(command);
                auto display = label;
                if (!parent.empty()) {
                    const std::array<core::translation::TypedFormatArgument, 2> arguments{
                        core::translation::ngFormatArgument(parent), core::translation::ngFormatArgument(label)};
                    auto formatted = core::translation::ngFormatTyped("[%s] %s", arguments);
                    try { display = compat::RetainedString(std::move(formatted)); }
                    catch (const std::bad_alloc&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    } catch (const std::length_error&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    }
                }
                const compat::SourceCString displayCString{display};
                compat::detail::ngSourceCStringPostconditions(display.view(),displayCString.data());
                if (ImGui::Selectable(displayCString.data(), i == gPaletteSelectedIndex)) {
                    gPaletteSelectedIndex = i;
                    self->executeCommand(command);
                    ImGui::EndChild(); return;
                }
                const auto shortcut = core::shortcut::ngShortcutFor(command);
                if (!shortcut.empty()) {
                    const compat::SourceCString sizeCString{shortcut};
                    compat::detail::ngSourceCStringPostconditions(shortcut.view(),sizeCString.data());
                    const auto size = ImGui::CalcTextSize(sizeCString.data());
                    const auto available = widgets::ngAvailableSpace();
                    const auto& style = ImGui::GetStyle();
                    const float right = available.x - size.x - style.ItemInnerSpacing.x - style.ScrollbarSize;
                    const float offset = right - ImGui::GetCursorPosX();
                    if (offset > 0) ImGui::SameLine(0, offset);
                    const compat::SourceCString textCString{shortcut};
                    compat::detail::ngSourceCStringPostconditions(shortcut.view(),textCString.data());
                    ImGui::Text(textCString.data());
                }
            }
        }
        ImGui::EndChild();
        if ((submitted || ImGui::IsKeyPressed(ImGuiKey_Enter)) && !filtered.empty()) {
            self->executeCommand(filtered[std::min(gPaletteSelectedIndex, filtered.size() - 1)]); return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { paletteClose(); return; }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) || ImGui::IsAnyItemHovered())) paletteClose();
    }, -1);
    return CommandResult::opCall(true);
}
void ListCommandCommand::executeCommand(std::shared_ptr<Command> command) {
    const auto context = core::shortcut::ngBuildExecutionContext();
    if (ngCommandAllowedInCurrentContext(command) && compat::ngRequireObject(command)->runnable(context)) {
        const auto result = ngRunCommand(command, context);
        if (compat::ngRequireObject(result)->succeeded) paletteClose();
    }
}
void ngInitPaletteCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::viewport::palette
