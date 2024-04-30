/*
    Copyright © 2020-2023, Inochi2D Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/

module creator.panels.resource;
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
import creator.widgets.output;

class View {
    string command;
    NodeOutput output;
public:
    this(string command, NodeOutput output) {
        this.command = command;
        this.output  = output;
    }
}

/**
    The Shell frame
*/
class ResourcePanel : Panel, CommandIssuer {
private:
    View[] history;
    bool forceUpdatePreview = false;
    uint historyIndex = 0;

protected:
    void execFilter(View view) {
        Selector selector = new Selector();
        selector.build(view.command);
        Resource[] nodes = selector.run();
        if (view.output is null)
            view.output = new NodeOutput(this);
        view.output.setResources(nodes);
    }

    override
    void onUpdate() {
        if (history.length == 0) {
            history ~= new View("*", null);
            execFilter(history[$-1]);
        }
        if (forceUpdatePreview) {
            execFilter(history[$-1]);
            forceUpdatePreview = false;
        }
        if (igBeginChild("ShellMain", ImVec2(0, -30), false)) {
            history[historyIndex].output.onUpdate();
        }
        igEndChild();
    }

public:
    this() {
        super("Resources", _("Resources"), false);
    }

    override
    void addCommand(string command) {
        history[$-1].command = history[$-1].command ~ command;
        forceUpdatePreview = true;
    }

    override
    void setCommand(string command) {
        history ~= new View(command, null);
        historyIndex ++;
        forceUpdatePreview = true;
    }
}

/**
    Generate logger frame
*/
mixin incPanel!ResourcePanel;

