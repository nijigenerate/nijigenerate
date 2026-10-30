#include "command_browser.hpp"
#include "command_browser_helpers.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <type_traits>
namespace nijigenerate::windows::command_browser {
using namespace detail;
void CommandBrowserWindow::drawOutputsTable(CommandInfo& info) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ImGui::BeginTable("outputs",4,ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) return;
    for (auto label : {"Field","Type","Meaning","Value"}) {
        const auto translated = core::translation::ngTranslateCString(label);
        ImGui::TableSetupColumn(translated.data());
    }
    ImGui::TableHeadersRow();
    // Defer each source operand until its own column, after preceding GUI calls.
    const auto row = [&](const char* field, auto type, const char* meaning, auto value) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(field);
        ImGui::TableSetColumnIndex(1);
        const auto typeValue = type();
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(typeValue)>, const char*>)
            ImGui::Text(typeValue);
        else ImGui::Text(sourceCString(typeValue).data());
        ImGui::TableSetColumnIndex(2);
        const auto translated = core::translation::ngTranslateCString(meaning);
        textWrappedInColumn(translated.data());
        ImGui::TableSetColumnIndex(3); ImGui::Text(sourceCString(value()).data());
    };
    row("type",[&] { return info.outputType.empty() ? staticText("CommandResult") : info.outputType; },
        "Declared return type",[&] {
            return lastHasResult ? (lastResultType.empty() ? info.outputType : lastResultType) : staticText("-");
        });
    row("succeeded",[] { return "bool"; },"True on success, false on failure.",
        [&] { return staticText(lastHasResult ? (lastSucceeded ? "true" : "false") : "-"); });
    row("message",[] { return "string"; },"Optional user-facing message.",
        [&] { return lastHasResult && !lastMessage.empty() ? lastMessage : staticText("-"); });
    if (info.isResource) {
        row("resourceType",[] { return "string"; },"Resource element type",
            [&] { return info.resourceType; });
        if (!info.resourceChange.empty())
            row("change",[] { return "string"; },"Created/Deleted/Loaded (hint)",
                [&] { return info.resourceChange; });
        const auto resourceType = info.resourceType.empty() ? staticText("Resource") : info.resourceType;
        const bool showCreated = info.resourceChange.empty() || info.resourceChange.view() == "Created";
        const bool showDeleted = info.resourceChange.empty() || info.resourceChange.view() == "Deleted";
        const bool showLoaded = info.resourceChange.empty() || info.resourceChange.view() == "Loaded";
        if (showCreated)
            row("created",[&] { return format("%s[]",resourceType); },"Resources created (may be empty)",
                [&] { return lastHasResult && !lastCreated.empty() ? join(lastCreated,", ") : staticText("-"); });
        if (showDeleted)
            row("deleted",[&] { return format("%s[]",resourceType); },"Resources deleted (may be empty)",
                [&] { return lastHasResult && !lastDeleted.empty() ? join(lastDeleted,", ") : staticText("-"); });
        if (showLoaded)
            row("loaded",[&] { return format("%s[]",resourceType); },"Resources loaded (may be empty)",
                [&] { return lastHasResult && !lastLoaded.empty() ? join(lastLoaded,", ") : staticText("-"); });
    } else if (info.isExResult) {
        row("result",[&] { return info.exResultType.empty() ? staticText("T") : info.exResultType; },
            "Wrapped ExCommand result type",[&] { return lastHasResult ? lastResultDetail : staticText("-"); });
    }
    ImGui::EndTable();
}
void CommandBrowserWindow::drawContextTable() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ImGui::BeginTable("context",3,ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) return;
    for (auto label : {"Context key","Type","Notes"}) {
        const auto translated = core::translation::ngTranslateCString(label);
        ImGui::TableSetupColumn(translated.data());
    }
    ImGui::TableHeadersRow();
    const auto row = [&](const char* field,const char* type,const char* note) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(field);
        ImGui::TableSetColumnIndex(1); ImGui::Text(type);
        ImGui::TableSetColumnIndex(2);
        const auto translated = core::translation::ngTranslateCString(note);
        ImGui::Text(translated.data());
    };
    row("parameters","uint[]","Optional. Parameter UUIDs. Used by many commands.");
    row("armedParameters","uint[]","Optional. Parameters to arm before execution.");
    row("nodes","uint[]","Optional. Node UUIDs. Used by inspector/node commands.");
    row("keyPoint","[uint,uint]","Optional. Key point index (x,y).");
    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("required?");
    ImGui::TableSetColumnIndex(1);
    const auto type = core::translation::ngTranslateCString("(per-command)");
    ImGui::Text(type.data());
    ImGui::TableSetColumnIndex(2);
    const auto note = core::translation::ngTranslateCString(
        "Requirements depend on each command's runnable/context check; not declared statically.");
    ImGui::Text(note.data());
    ImGui::EndTable();
}
}
