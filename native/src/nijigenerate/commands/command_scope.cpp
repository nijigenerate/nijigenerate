#include "command_scope_command.hpp"
#include "concrete_catalog.hpp"
#include "register_command.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <common/string_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::commands {
namespace {
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using String = compat::RetainedString;
using StringSet = nicxlive::core::common::StringKeyMap<bool>;

template<class Operation> auto allocateString(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<std::size_t N> core::settings::SettingsString key(const char (&text)[N]) {
    return core::settings::SettingsString(String::fromStaticLiteral(text));
}
Json stringValue(String text) { return Json(core::settings::SettingsString(text)); }

// Source foreach(char) passes each unsigned byte to std.uni.isAlphaNum(dchar).
// This is the installed Phobos Alphabetic|Nd|Nl|No table restricted to 0..255.
bool isToolNameAlphaNum(unsigned char value) {
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z') || value == 0xAA
        || (value >= 0xB2 && value <= 0xB3) || value == 0xB5
        || (value >= 0xB9 && value <= 0xBA) || (value >= 0xBC && value <= 0xBE)
        || (value >= 0xC0 && value <= 0xD6) || (value >= 0xD8 && value <= 0xF6)
        || value >= 0xF8;
}
String mcpToolName(String commandId) {
    if (commandId.empty()) return {};
    return allocateString([&] {
        std::string result;
        for (unsigned char ch : commandId.view())
            result += (isToolNameAlphaNum(ch) || ch == '_' || ch == '-') ? static_cast<char>(ch) : '_';
        return String(std::move(result));
    });
}
Json scopeToJson(std::shared_ptr<CommandScope> scope) {
    Object result;
    result[key("id")] = stringValue(compat::ngRequireObject(scope)->id());
    result[key("type")] = stringValue(compat::ngRequireObject(scope)->typeName());
    result[key("description")] = stringValue(compat::ngRequireObject(scope)->description());
    return Json(result);
}
Json transitionsToJson(compat::Slice<CommandScopeTransition> transitions, StringSet& availableCommandIds) {
    Array result;
    for (const auto transition : transitions) {
        Object entry;
        entry[key("commandId")] = stringValue(transition.commandId);
        entry[key("tool")] = stringValue(mcpToolName(transition.commandId));
        entry[key("description")] = stringValue(transition.description);
        entry[key("available")] = Json(availableCommandIds.contains(transition.commandId.view()));
        result.append(Json(entry));
    }
    return Json(result);
}
struct AvailableCommand {
    String commandId;
    String tool;
    String label;
    String description;
};
}
GetCurrentCommandScopeCommand::SourceConstructorArguments
GetCurrentCommandScopeCommand::sourceConstructorArguments() {
    auto label = core::translation::ngTranslateRetained(String::fromStaticLiteral("Get Current Command Scope"));
    auto description = core::translation::ngTranslateRetained(String::fromStaticLiteral(
        "Return the active command scope, allowed MCP commands, and commands that finish or cancel the scope."));
    return {std::move(label), std::move(description)};
}
GetCurrentCommandScopeCommand::GetCurrentCommandScopeCommand()
    : GetCurrentCommandScopeCommand(sourceConstructorArguments()) {}
GetCurrentCommandScopeCommand::GetCurrentCommandScopeCommand(SourceConstructorArguments arguments)
    : ExCommand{arguments.label, arguments.description} {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(GetCurrentCommandScopeCommand),
        String::fromStaticLiteral("nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand"));
}
void GetCurrentCommandScopeCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    detail::prepareCommandResultClass<GetCurrentCommandScopeCommand>();
}
std::shared_ptr<CommandResult> GetCurrentCommandScopeCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runTyped(context);
}
std::shared_ptr<GetCurrentCommandScopeCommand::Result> GetCurrentCommandScopeCommand::runTyped(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    compat::Slice<AvailableCommand> entries;
    StringSet availableIds, registeredTools;
    ngForEachCommandMap([&](const auto& table) {
        const auto evaluatedTable = table;
        for (const auto [sourceKey, command] : evaluatedTable) {
            if (!command || !command->mcpExposed() || !ngCommandAllowedInCurrentContext(command)) continue;
            auto sourceId = ngCommandIdFromKey(sourceKey);
            const String id = allocateString([&] { return String(std::move(sourceId)); });
            auto tool = mcpToolName(id);
            if (registeredTools.contains(tool.view())) {
                const auto typeSuffix = mcpToolName(ngCommandSourceClassName(command));
                tool = allocateString([&] {
                    return String(std::string(tool.view()) + "_" + std::string(typeSuffix.view()));
                });
                std::size_t suffix = 1;
                while (registeredTools.contains(tool.view())) {
                    tool = allocateString([&] {
                        return String(std::string(tool.view()) + "_" + std::to_string(suffix));
                    });
                    ++suffix;
                }
            }
            registeredTools[tool] = true;
            availableIds[id] = true;
            const auto label = command->label();
            const auto description = command->description();
            entries.append({id, tool, label, description});
        }
    });
    if (entries.size() > 1) std::sort(entries.begin(), entries.end(),
        [](const AvailableCommand& a, const AvailableCommand& b) { return a.tool.view() < b.tool.view(); });
    Array available;
    for (const auto command : entries) {
        Object entry;
        entry[key("commandId")] = stringValue(command.commandId);
        entry[key("tool")] = stringValue(command.tool);
        entry[key("label")] = stringValue(command.label);
        entry[key("description")] = stringValue(command.description);
        available.append(Json(entry));
    }
    const auto scopes = ngCommandScopeStackSnapshot();
    Array stack;
    for (const auto scope : scopes) stack.append(scopeToJson(scope));
    const auto current = ngCurrentCommandScope();
    Object lifecycle;
    lifecycle[key("finish")] = transitionsToJson(compat::ngRequireObject(current)->completionCommands(), availableIds);
    lifecycle[key("cancel")] = transitionsToJson(compat::ngRequireObject(current)->cancellationCommands(), availableIds);
    Object result;
    result[key("scope")] = scopeToJson(current);
    result[key("scopeStack")] = Json(stack);
    result[key("availableCommands")] = Json(available);
    result[key("lifecycle")] = Json(lifecycle);
    result[key("toolSpecification")] = stringValue(String::fromStaticLiteral(
        "Use tools/list to read the input schema for each returned tool name."));
    return Result::opCall(true, Json(result));
}
void ngInitCommandScopeCommands() {
    detail::ScopeInitialization::run<detail::ScopeInitializationBinding>();
}
} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::AvailableCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.command_scope.AvailableCommand)";
    inline static constexpr char unqualifiedArgument[] = "(AvailableCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.command_scope.AvailableCommand";
    inline static constexpr char unqualifiedElement[] = "AvailableCommand";
};
}
