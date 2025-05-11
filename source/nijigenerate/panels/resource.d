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
import nijigenerate.panels.nodes;
import nijigenerate.panels.parameters;
import nijigenerate.utils;
import nijigenerate.widgets;
import nijigenerate.widgets.output;
import fghj;
//import std.stdio;

private {
static string ResourcePanelPath = "com.github.nijigenerate.nijigenerate.ResourcePanel";
ResourcePanel singleton;
static this() {
    singleton = new ResourcePanel;
    incAddPanel(singleton);
}
}

void incLoadResourcePanel(Puppet puppet) {
    if (ResourcePanelPath in puppet.extData && puppet.extData[ResourcePanelPath].length > 0) {
        auto jsonData = parseJson(cast(string)puppet.extData[ResourcePanelPath]);

        if (singleton) {
            deserialize!ResourcePanelConfig(jsonData);            
        }
    }
}

void incDumpResourcePanelPath(Puppet puppet) {
    if (singleton) {
        auto app = appender!(char[]);
        auto serializer = inCreateSerializer(app);
        auto config = new ResourcePanelConfig;
        serializer.serializeValue(config);
        serializer.flush();
        puppet.extData[ResourcePanelPath] = cast(ubyte[])app.data;
    }
}

void ngInitResourcePanel() {
//    incRegisterLoadFunc(&incLoadResourcePanel);
    incRegisterSaveFunc(&incDumpResourcePanelPath);
}

@TypeId("ResourcePanel")
class ResourcePanelConfig : ISerializable {

    void serialize(S)(ref S serializer) {
        if (!singleton || singleton.history.length == 0) return;
        auto output = cast(IconTreeOutput)singleton.history[0].output;
        if (output) {
            auto state = serializer.objectBegin();
                serializer.putKey("nextInHorizontal");
                auto arr1 = serializer.arrayBegin();
                    foreach(uuid; output.layout.keys()) {
                        if (output.layout[uuid].nextInHorizontal) {
                            serializer.elemBegin;
                            serializer.serializeValue(uuid);
                        }
                    }
                serializer.arrayEnd(arr1);
                serializer.putKey("folded");
                auto arr2 = serializer.arrayBegin();
                    foreach(uuid; output.layout.keys()) {
                        if (output.layout[uuid].folded) {
                            serializer.elemBegin;
                            serializer.serializeValue(uuid);
                        }
                    }
                serializer.arrayEnd(arr2);
            serializer.objectEnd(state);
        }
    }

    SerdeException deserializeFromFghj(Fghj data) {
        import std.algorithm.searching: count;
        if (data.isEmpty) return null;

        if (singleton.history.length == 0) {
            // TBD
        }
        
        auto view = singleton.history[0];
        auto output = cast(IconTreeOutput)view.output;

        if (!data["nextInHorizontal"].isEmpty) {
            auto elements = data["nextInHorizontal"].byElement;
            while(!elements.empty) {
                uint uuid;
                elements.front.deserializeValue(uuid);
                elements.popFront;
                output.layout.require(uuid);
                output.layout[uuid].nextInHorizontal = true;
            }
        }

        if (!data["folded"].isEmpty) {
            auto elements = data["folded"].byElement;
            while(!elements.empty) {
                uint uuid;
                elements.front.deserializeValue(uuid);
                elements.popFront;
                output.layout.require(uuid);
                output.layout[uuid].folded = true;
            }
        }

        return null;
    }
}

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
class ResourcePanel : Panel, CommandIssuer {
package:
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
            incLoadResourcePanel(activePuppet);
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
        // temp variables
        float scrollDelta = 0;
        auto avail = incAvailableSpace();

        // Get the screen position of our node window
        // as well as the size for the drag/drop scroll
        ImVec2 screenPos;
        igGetCursorScreenPos(&screenPos);
        ImRect crect = ImRect(
            screenPos,
            ImVec2(screenPos.x+avail.x, screenPos.y+avail.y)
        );

        // Handle figuring out whether the user is trying to scroll the list via drag & drop
        // We're only peeking in to the contents of the payload.
        incBeginDragDropFake();
            auto data = igAcceptDragDropPayload("_PUPPETNTREE", ImGuiDragDropFlags.AcceptPeekOnly | ImGuiDragDropFlags.SourceAllowNullID);
            if (igIsMouseDragging(ImGuiMouseButton.Left) && data && data.Data) {
                ImVec2 mousePos;
                igGetMousePos(&mousePos);

                // If mouse is inside the window
                if (mousePos.x > crect.Min.x && mousePos.x < crect.Max.x) {
                    float scrollSpeed = (4*256)*deltaTime();

                    if (mousePos.y < crect.Min.y+32 && mousePos.y >= crect.Min.y) scrollDelta = -scrollSpeed;
                    if (mousePos.y > crect.Max.y-32 && mousePos.y <= crect.Max.y) scrollDelta = scrollSpeed;
                }
            }
        incEndDragDropFake();

        int buttonBarHeight = -34;
        if (igBeginChild("ShellMain", ImVec2(0, buttonBarHeight), false)) {
            auto window = igGetCurrentWindow();
            igSetScrollY(window.Scroll.y+scrollDelta);
            history[historyIndex].output.onUpdate();
        }
        igEndChild();
        auto spacing = igGetStyle().ItemSpacing;
        igGetStyle().ItemSpacing = ImVec2(0, 0);
        if (igBeginPopup("###AddResource")) {
            igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("New Node"));
            igSeparator();
            igDummy(ImVec2(0, 6));
            ngAddNodeMenu();
            igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("New Parameter"));
            igSeparator();
            igDummy(ImVec2(0, 6));
            incParameterMenuContents(incActivePuppet().parameters);
            igEndPopup();
        }
        if (incButtonColored("\ue145", ImVec2(30, 30))) { //New
            igOpenPopup("###AddResource");
        }
//        igSameLine();
//        incButtonColored("\ue872"); //Delete
//        igSameLine();
//        incButtonColored("\ue14d"); //Copy
//        igSameLine();
//        incButtonColored("\ue14f"); //Paste
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
//mixin incPanel!ResourcePanel;

