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
import creator.ext;
import creator.core;
import creator.core.selector;
import creator.panels;
import creator.utils;
import creator.widgets;
import creator.widgets.output;

class View {
    string command;
    TreeOutput output;
public:
    this(string command, TreeOutput output) {
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
    Puppet activePuppet;
    Parameter armedParameter;
    ViewOutput views;

protected:
    void execFilter(View view) {
        Selector selector = new Selector();
        selector.build(view.command ~ (armedParameter? ", Binding:active": ""));
        Resource[] nodes = selector.run();
        if (view.output is null)
            view.output = new IconTreeOutput(this);
        view.output.setResources(nodes);
    }

    void notifyChange(Node target, NotifyReason reason) {
        import std.stdio;
        if (reason == NotifyReason.StructureChanged) {
            writefln("changed %s", target.name);
            forceUpdatePreview = true;
        }
    }

    override
    void onUpdate() {
        if (incActivePuppet() != activePuppet) {
            activePuppet = incActivePuppet();
            if (activePuppet) {
                import std.stdio;
                Node rootNode = activePuppet.root;
                rootNode.addNotifyListener(&notifyChange);
            }
            forceUpdatePreview = true;
        }
        if (incArmedParameter() != armedParameter) {
            armedParameter = incArmedParameter();
            forceUpdatePreview = true;
        }
        if (views is null) {
            views = new ViewOutput(this);
        }
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
        auto spacing = igGetStyle().ItemSpacing;
        igGetStyle().ItemSpacing = ImVec2(0, 0);
        igButton("\ue145");
        igSameLine();
        igButton("\ue872");
        igSameLine();
        igButton("\ue14d");
        igSameLine();
        igButton("\ue14f");
        igGetStyle().ItemSpacing = spacing;
        views.onUpdate();
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

    override
    void addPopup(Resource res) {
        views.addResources([res]);
    }
}

/**
    Generate logger frame
*/
mixin incPanel!ResourcePanel;

