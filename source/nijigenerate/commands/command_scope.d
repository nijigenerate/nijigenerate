module nijigenerate.commands.command_scope;

import nijigenerate.commands.base;
import std.algorithm.sorting : sort;
import std.array : appender;
import std.conv : to;
import std.json : JSONValue;
import std.uni : isAlphaNum;
import i18n;

enum CommandScopeCommand {
    GetCurrentCommandScope,
}

Command[CommandScopeCommand] commands;

private string mcpToolName(string commandId) {
    auto result = appender!string();
    foreach (ch; commandId) {
        result ~= (isAlphaNum(ch) || ch == '_' || ch == '-') ? ch : '_';
    }
    return result.data;
}

private JSONValue scopeToJson(CommandScope commandScope) {
    JSONValue[string] result;
    result["id"] = JSONValue(commandScope.id());
    result["type"] = JSONValue(commandScope.typeName());
    result["description"] = JSONValue(commandScope.description());
    return JSONValue(result);
}

private JSONValue transitionsToJson(
    CommandScopeTransition[] transitions,
    ref bool[string] availableCommandIds
) {
    JSONValue[] result;
    foreach (transition; transitions) {
        JSONValue[string] entry;
        entry["commandId"] = JSONValue(transition.commandId);
        entry["tool"] = JSONValue(mcpToolName(transition.commandId));
        entry["description"] = JSONValue(transition.description);
        entry["available"] = JSONValue((transition.commandId in availableCommandIds) !is null);
        result ~= JSONValue(entry);
    }
    return JSONValue(result);
}

private struct AvailableCommand {
    string commandId;
    string tool;
    string label;
    string description;
}

@ShortcutHidden
@CommandScopes!(GlobalCommandScope)()
class GetCurrentCommandScopeCommand : ExCommand!() {
    this() {
        super(
            _("Get Current Command Scope"),
            _("Return the active command scope, allowed MCP commands, and commands that finish or cancel the scope.")
        );
    }

    override ExCommandResult!JSONValue run(Context ctx) {
        import nijigenerate.commands : AllCommandMaps;

        AvailableCommand[] availableCommands;
        bool[string] availableCommandIds;
        bool[string] registeredToolNames;
        static foreach (AA; AllCommandMaps) {
            foreach (key, command; AA) {
                if (command is null || !command.mcpExposed() ||
                    !ngCommandAllowedInCurrentContext(command)) {
                    continue;
                }

                auto commandId = ngCommandIdFromKey(key);
                auto tool = mcpToolName(commandId);
                if (tool in registeredToolNames) {
                    tool ~= "_" ~ mcpToolName(typeid(command).name);
                    size_t suffix = 1;
                    while (tool in registeredToolNames) {
                        tool ~= "_" ~ suffix.to!string;
                        suffix++;
                    }
                }
                registeredToolNames[tool] = true;
                availableCommandIds[commandId] = true;
                availableCommands ~= AvailableCommand(
                    commandId,
                    tool,
                    command.label(),
                    command.description()
                );
            }
        }
        availableCommands.sort!((a, b) => a.tool < b.tool);

        JSONValue[] available;
        foreach (command; availableCommands) {
            JSONValue[string] entry;
            entry["commandId"] = JSONValue(command.commandId);
            entry["tool"] = JSONValue(command.tool);
            entry["label"] = JSONValue(command.label);
            entry["description"] = JSONValue(command.description);
            available ~= JSONValue(entry);
        }

        auto stack = ngCommandScopeStackSnapshot();
        JSONValue[] stackJson;
        foreach (commandScope; stack) stackJson ~= scopeToJson(commandScope);

        auto current = ngCurrentCommandScope();
        JSONValue[string] lifecycle;
        lifecycle["finish"] = transitionsToJson(current.completionCommands(), availableCommandIds);
        lifecycle["cancel"] = transitionsToJson(current.cancellationCommands(), availableCommandIds);

        JSONValue[string] result;
        result["scope"] = scopeToJson(current);
        result["scopeStack"] = JSONValue(stackJson);
        result["availableCommands"] = JSONValue(available);
        result["lifecycle"] = JSONValue(lifecycle);
        result["toolSpecification"] = JSONValue(
            "Use tools/list to read the input schema for each returned tool name."
        );
        return ExCommandResult!JSONValue(true, JSONValue(result));
    }
}

void ngInitCommands(T)() if (is(T == CommandScopeCommand)) {
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!CommandScopeCommand) {
        mixin(registerCommand!(name));
    }
}
