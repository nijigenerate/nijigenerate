/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/

module nijigenerate.panels.resource;
import std.array;
import std.string;
import std.algorithm;
import i18n;
import nijilive;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.core;
import nijigenerate.core.selector;
import nijigenerate.core.selector.resource: Resource, ResourceInfo, ResourceType;
import nijigenerate.panels;
import nijigenerate.utils;
import nijigenerate.widgets;
import nijigenerate.widgets.output;

class View {
    string command;
    TreeStore store;
    ListOutput output;
public:
    this(string command, ListOutput output) {
        this.command = command;
        this.output  = output;
    }
}

/**
    The Shell frame
*/
@TypeId("ResourcePanel")
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
        if (view.store is null)
            view.store = new TreeStore;
        view.store.setResources(nodes);
        if (view.output is null)
            view.output = new IconTreeOutput(view.store, this);
        views.refresh(nodes);
    }

    void notifyChange(Node target, NotifyReason reason) {
        if (reason == NotifyReason.StructureChanged) {
            forceUpdatePreview = true;
        }
    }

    override
    void onUpdate() {
        if (incActivePuppet() != activePuppet) {
            activePuppet = incActivePuppet();
            if (activePuppet) {
                Node rootNode = activePuppet.root;
                rootNode.addNotifyListener(&notifyChange);
            }
            foreach (item; history) {
                item.output.reset();
            }
            if (views)
                views.reset();
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
        if (igBeginChild("ShellMain", ImVec2(0, -34), false)) {
            history[historyIndex].output.onUpdate();
        }
        igEndChild();
        auto spacing = igGetStyle().ItemSpacing;
        igGetStyle().ItemSpacing = ImVec2(0, 0);
        incButtonColored("\ue145");
        igSameLine();
        incButtonColored("\ue872");
        igSameLine();
        incButtonColored("\ue14d");
        igSameLine();
        incButtonColored("\ue14f");
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

    void serialize(S)(ref S serializer) {
        auto view = cast(IconTreeOutput)history[0];
        if (view) {
            auto state = serializer.objectBegin();
                serializer.putKey("nextInHorizontal");
                auto arr = serializer.arrayBegin();
                    foreach(uuid; view.layout.keys()) {
                        serializer.elemBegin;
                        serializer.serializeValue(uuid);
                    }
                serializer.arrayEnd(arr);
            serializer.objectEnd(state);
        }
    }

    SerdeException deserializeFromFghj(Fghj data) {
        import std.stdio : writeln;
        import std.algorithm.searching: count;
        if (data.isEmpty) return null;

        if (history.length == 0) {
            // TBD
        }
        
        auto view = history[0];
        auto output = cast(IconTreeOutput)view.output;

        auto elements = data["nextInHorizontal"].byElement;
        while(!elements.empty) {
            uint uuid;
            elements.front.deserializeValue(uuid);
            elements.popFront;
            output.layout.require(uuid);
            output.layout[uuid].nextInHorizontal = true;
        }

        return null;
    }

}

/**
    Generate logger frame
*/
mixin incPanel!ResourcePanel;

