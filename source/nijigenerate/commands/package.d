module nijigenerate.commands;
import nijigenerate.commands.binding.binding;
import nijigenerate.commands.node.node;
import nijigenerate.commands.parameter.animedit;
import nijigenerate.commands.parameter.group;
import nijigenerate.commands.parameter.param;
import nijigenerate.commands.parameter.paramedit;
import std.meta : AliasSeq;

alias bindingCommands = nijigenerate.commands.binding.binding.commands;
alias nodeCommands = nijigenerate.commands.node.node.commands;
alias animEditCommands = nijigenerate.commands.parameter.animedit.commands;
alias groupCommands = nijigenerate.commands.parameter.group.commands;
alias paramCommands = nijigenerate.commands.parameter.param.commands;
alias paramEditCommands = nijigenerate.commands.parameter.paramedit.commands;

/*
alias AllCommands = AliasSeq!(
    bindingCommands,
    nodeCommands,
    animEditCommands,
    groupCommands,
    paramCommands,
    paramEditCommands
);
private {
    static this() {
        import std.stdio;
        import std.conv;
        foreach (cmds; AllCommands) {
            // cmds は連想配列(enumType => valueType)
            foreach (k, v; cmds) {
                writeln("[", typeof(k).stringof, "] ", k.to!string, " => ", v);
            }
        }
    }
}
*/