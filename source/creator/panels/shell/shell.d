/*
    Copyright © 2020-2023, Inochi2D Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/

module creator.panels.shell.shell;
import std.array;
import std.string;
import std.algorithm;
import i18n;
import inochi2d;
import creator;
import creator.core;
import creator.core.selector;
import creator.panels;
import creator.utils;
import creator.widgets;
import creator.panels.shell;

class Command {
    string command;
    Output output;
public:
    this(string command, Output output) {
        this.command = command;
        this.output  = output;
    }
}

/**
    The Shell frame
*/
class ShellPanel : Panel {
private:
    string command;
    Command[] history;

    Output latestOutput;

protected:
    override
    void onUpdate() {
        if (igBeginChild("ShellMain", ImVec2(0, -30), false)) {
            float heightUnit = igGetTextLineHeightWithSpacing();
            ImVec2 avail = incAvailableSpace();
            avail.y = heightUnit * 3;

            if (incInputTextMultiline("(Command)", command, avail)) {
                string newCommand = (cast(string)this.command).dup.toStringz.fromStringz;
                try {
                    Selector selector = new Selector();
                    selector.build(newCommand);
                    Resource[] nodes = selector.run();
                    if (newCommand == "" || nodes.length > 0) {
                        if (!latestOutput) {
                            latestOutput = new NodeOutput();
                        }
                        (cast(NodeOutput)latestOutput).setNodes(nodes);
                    }
                } catch (std.utf.UTFException e) {}
            }
            if (latestOutput)
                latestOutput.onUpdate();

            if (igIsKeyPressed(ImGuiKey.Enter)) {
                string newCommand = (cast(string)this.command).dup;
                Selector selector = new Selector();
                selector.build(newCommand);
                Resource[] nodes = selector.run();
                auto output = new NodeOutput();
                output.setNodes(nodes);
                this.command = "";
                history ~= new Command(newCommand, output);
                latestOutput = null;
            }
            foreach_reverse (i, c; history) {
                igText("[%d] %s".format(i, c.command).toStringz);
                if (incBeginCategory("Output [%d]".format(i).toStringz, IncCategoryFlags.DefaultClosed)) {
                    c.output.onUpdate();
                }
                incEndCategory();
            }
        }
        igEndChild();
    }

public:
    this() {
        super("Shell", _("Shell"), false);
    }
}

/**
    Generate logger frame
*/
mixin incPanel!ShellPanel;

