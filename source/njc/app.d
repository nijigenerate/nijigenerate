module app;

import std.stdio : stderr, writeln;

import njc.commands : runCommand;
import njc.config : parseOptions, usage;

int main(string[] args) {
    try {
        auto options = parseOptions(args);
        if (options.help) {
            writeln(usage());
            return 0;
        }
        return runCommand(options);
    } catch (Exception e) {
        stderr.writeln("njc: ", e.msg);
        return 1;
    }
}
