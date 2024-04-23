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
    string nextCommand = null;
    bool forceUpdatePreview = false;
    Command[] history;

    Output latestOutput;

protected:
    void updatePreview() {
        string newCommand = (cast(string)this.command).dup.toStringz.fromStringz;
        try {
            Selector selector = new Selector();
            selector.build(newCommand);
            Resource[] nodes = selector.run();
            if (newCommand == "" || nodes.length > 0) {
                if (!latestOutput) {
                    latestOutput = new NodeOutput(this);
                }
                (cast(NodeOutput)latestOutput).setNodes(nodes);
            }
        } catch (std.utf.UTFException e) {}
    }

    override
    void onUpdate() {
        if (nextCommand) {
            command = nextCommand;
            nextCommand = null;
        }
        ImVec2 avail = incAvailableSpace();
        if (incInputText("(Command)", avail.x, command) || forceUpdatePreview) {
            updatePreview();
            forceUpdatePreview = false;
        }
        if (igIsKeyPressed(ImGuiKey.Enter)) {
            string newCommand = (cast(string)this.command).dup;
            Selector selector = new Selector();
            selector.build(newCommand);
            Resource[] nodes = selector.run();
            auto output = new NodeOutput(this);
            output.setNodes(nodes);
            this.command = "";
            history ~= new Command(newCommand, output);
            latestOutput = null;
            setCommand(" ");
        }
        if (igBeginChild("ShellMain", ImVec2(0, -30), false)) {

            if (latestOutput)
                latestOutput.onUpdate();

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

    void addCommand(string command) {
        nextCommand = this.command ~ command ~ '\0';
        forceUpdatePreview = true;
    }

    void setCommand(string command) {
        nextCommand = command ~ '\0';
        forceUpdatePreview = true;
    }
}

/**
    Generate logger frame
*/
mixin incPanel!ShellPanel;

