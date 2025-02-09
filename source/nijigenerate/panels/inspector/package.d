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

Inspector!Node[] inspectors;
Inspector!Puppet[] puppetInspectors;

bool[string] isCommonAttr;

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

    ngRegisterInspector!(ModelEditSubMode.Layout, Puppet)();
}

}


void ngRegisterInspector(ModelEditSubMode mode, T: Node)() {
    inspectors ~= new NodeInspector!(mode, T);
}

void ngRegisterInspector(ModelEditSubMode mode, T: Puppet)() {
    puppetInspectors ~= new PuppetInspector!(mode, T);
}

void ngInspector(T: Node, Args...)(T target, Args args) {
    auto mode = ngModelEditSubMode;
    if (mode == ModelEditSubMode.Layout) {
        incModelModeHeader(target);
    } else if (mode == ModelEditSubMode.Deform) {
        incCommonNonEditHeader(target);
    }
    foreach (ins; inspectors) {
        static if (args.length == 0) {
            ins.inspect(target, mode);
        } else if (args.length == 2) {
            ins.inspect(target, mode, args[0], args[1]);
        }
    }
}

void ngInspector(T: Puppet, Args...)(T target, Args args) {
    auto mode = ngModelEditSubMode;
    foreach (ins; puppetInspectors) {
        static if (args.length == 0) {
            ins.inspect(target, mode);
        } else if (args.length == 2) {
            ins.inspect(target, mode, args[0], args[1]);
        }
    }
}

void ngUpdateAttributeCache(Node node) {
}

/**
    The inspector panel
*/
class InspectorPanel : Panel {
private:
    Puppet activePuppet = null;

protected:
    void notifyChange(Node target, NotifyReason reason) {
        if (reason == NotifyReason.StructureChanged || reason == NotifyReason.AttributeChanged) {
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
        }

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
                        auto subMode = ngModelEditSubMode();
                        Parameter param = incArmedParameter();
                        vec2u cursor = param? param.findClosestKeypoint(): vec2u.init;
                        ngInspector(node, param, cursor);
                    break;
                    default:
                        incCommonNonEditHeader(node);
                        break;
                }
            } else {
                ngInspector(incActivePuppet());
            }
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


//
//  MODEL MODE
//
