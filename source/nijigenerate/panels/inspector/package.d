/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.inspector;
import nijigenerate;
import nijigenerate.panels;
public import nijigenerate.panels.inspector.common;
public import nijigenerate.panels.inspector.node;
public import nijigenerate.panels.inspector.puppet;
public import nijigenerate.panels.inspector.drawable;
public import nijigenerate.panels.inspector.part;
public import nijigenerate.panels.inspector.composite;
public import nijigenerate.panels.inspector.simplephysics;
public import nijigenerate.panels.inspector.meshgroup;
public import nijigenerate.panels.inspector.camera;
public import nijigenerate.panels.inspector.pathdeform;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijilive;
import i18n;
import std.utf;
import std.string;
import std.traits;
import std.array;

private {

void delegate(Node)[] layoutInspectors;
void delegate(Node, Parameter, vec2u)[] deformInspectors;

void initInspectors() {
    ngRegisterInspector!(ModelEditSubMode.Deform, Node)();
    ngRegisterInspector!(ModelEditSubMode.Deform, Part)();
    ngRegisterInspector!(ModelEditSubMode.Deform, Composite)();
    ngRegisterInspector!(ModelEditSubMode.Deform, SimplePhysics)();
    ngRegisterInspector!(ModelEditSubMode.Deform, PathDeformer)();

    ngRegisterInspector!(ModelEditSubMode.Layout, Node)();
    ngRegisterInspector!(ModelEditSubMode.Layout, ExCamera)();
    ngRegisterInspector!(ModelEditSubMode.Layout, Composite)();
    ngRegisterInspector!(ModelEditSubMode.Layout, Drawable)();
    ngRegisterInspector!(ModelEditSubMode.Layout, Part)();
    ngRegisterInspector!(ModelEditSubMode.Layout, SimplePhysics)();
    ngRegisterInspector!(ModelEditSubMode.Layout, MeshGroup)();
    ngRegisterInspector!(ModelEditSubMode.Layout, PathDeformer)();
}

}


void ngRegisterInspector(ModelEditSubMode mode, T)() {
    alias Inspector = incInspector!(mode, T);
    static if (mode == ModelEditSubMode.Layout) {
        layoutInspectors ~= (Node node) {
            if (auto target = cast(T)node)
                Inspector(target);
        };
    }
    static if (mode == ModelEditSubMode.Deform) {
        deformInspectors ~= (Node node, Parameter param, vec2u cursor) {
            if (auto target = cast(T)node)
                Inspector(target, param, cursor);
        };
    }
}

void neInspector(ModelEditSubMode mode, Args...)(Node node, Args args) {
    static if (mode == ModelEditSubMode.Layout) {
        foreach (ins; layoutInspectors) {
            ins(node);
        }
    }
    static if (mode == ModelEditSubMode.Deform) {
        foreach (ins; deformInspectors) {
            ins(node, args[0], args[1]);
        }
    }
}

/**
    The inspector panel
*/
class InspectorPanel : Panel {
private:


protected:
    override
    void onUpdate() {
        if (incEditMode == EditMode.VertexEdit) {
            incLabelOver(_("In vertex edit mode..."), ImVec2(0, 0), true);
            return;
        }

        auto nodes = incSelectedNodes();
        if (nodes.length == 1) {
            Node node = nodes[0];
            if (node !is null && node != incActivePuppet().root) {

                // Per-edit mode inspector drawers
                switch(incEditMode()) {
                    case EditMode.ModelEdit:
                        if (incArmedParameter()) {
                            Parameter param = incArmedParameter();
                            vec2u cursor = param.findClosestKeypoint();
                            incCommonNonEditHeader(node);
                            neInspector!(ModelEditSubMode.Deform)(node, param, cursor);
                        } else {
                            incModelModeHeader(node);
                            neInspector!(ModelEditSubMode.Layout)(node);
                        }
                    
                    break;
                    default:
                        incCommonNonEditHeader(node);
                        break;
                }
            } else incInspector!(ModelEditSubMode.Layout)(incActivePuppet());
        } else if (nodes.length == 0) {
            incLabelOver(_("No nodes selected..."), ImVec2(0, 0), true);
        } else {
            incLabelOver(_("Can only inspect a single node..."), ImVec2(0, 0), true);
        }
    }

public:
    this() {
        super("Inspector", _("Inspector"), true);
        activeModes = EditMode.ModelEdit;
        initInspectors();
    }
}

/**
    Generate logger frame
*/
mixin incPanel!InspectorPanel;



//
// COMMON
//

void incCommonNonEditHeader(Node node) {
    // Top level
    igPushID(node.uuid);
        string typeString = "%s".format(incTypeIdToIcon(node.typeId()));
        auto len = incMeasureString(typeString);
        incText(node.name);
        igSameLine(0, 0);
        incDummy(ImVec2(-len.x, len.y));
        igSameLine(0, 0);
        incText(typeString);
    igPopID();
    igSeparator();
}

//
//  MODEL MODE
//
