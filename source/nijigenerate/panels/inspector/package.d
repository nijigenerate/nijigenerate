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
import std.algorithm;
import std.string;
import std.traits;
import std.array;

private {

Inspector!Node delegate()[] nodeInspectors;

void initInspectors() {
    ngRegisterInspector!(ModelEditSubMode.Deform, Node)();
    ngRegisterInspector!(ModelEditSubMode.Deform, Part)();
    ngRegisterInspector!(ModelEditSubMode.Deform, Composite)();
    ngRegisterInspector!(ModelEditSubMode.Deform, SimplePhysics)();

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
    nodeInspectors ~= () => new NodeInspector!(mode, T)([], mode);
}

void ngRegisterInspector(ModelEditSubMode mode, T: Puppet)() {
    nodeInspectors ~= () => new PuppetInspector([], mode);
}


InspectorHolder!Node ngNodeInspector(Node[] targets) {
    auto mode = ngModelEditSubMode;
    auto result = new InspectorHolder!Node(targets, mode);
    result.setInspectors(nodeInspectors.map!((i) => i()).array);
    return result;
}


/**
    The inspector panel
*/
class InspectorPanel : Panel {
private:
    Puppet activePuppet = null;
    Project activeProject = null;

    InspectorHolder!Node activeNodeInspectors;

protected:
    void onChange(Node target, NotifyReason reason) {
        if (reason == NotifyReason.StructureChanged || reason == NotifyReason.AttributeChanged) {
            if (activeNodeInspectors)
                activeNodeInspectors.capture(activeNodeInspectors.getTargets());
        }
    }

    void onSelectionChanged(Node[] nodes) {
        auto mode = ngModelEditSubMode;
        activeNodeInspectors = new InspectorHolder!Node(nodes, mode);
        activeNodeInspectors.setInspectors(nodeInspectors.map!((i) => i()).array);

    }

    override
    void onUpdate() {
        auto subMode = ngModelEditSubMode();
        if (incActiveProject() != activeProject) {
            activeProject = incActiveProject();
            activeProject.SelectionChanged.connect(&onSelectionChanged);
        }
        if (incActivePuppet() != activePuppet) {
            activePuppet = incActivePuppet();
            if (activePuppet) {
                Node rootNode = activePuppet.root;
                rootNode.addNotifyListener(&onChange);
            }
        }
        if (activeNodeInspectors) {
            activeNodeInspectors.subMode = subMode;
        }

        if (incEditMode == EditMode.VertexEdit) {
            incLabelOver(_("In vertex edit mode..."), ImVec2(0, 0), true);
            return;
        }

        auto nodes = incSelectedNodes();
        if (nodes.length > 0) {
            // Per-edit mode inspector drawers
            switch(incEditMode()) {
                case EditMode.ModelEdit:
                    Parameter param = incArmedParameter();
                    vec2u cursor = param? param.findClosestKeypoint(): vec2u.init;
                    if (activeNodeInspectors)
                        activeNodeInspectors.inspect(param, cursor);
                break;
                default:
                    incCommonNonEditHeader(nodes[0]);
                    break;
            }
        } else {
            incLabelOver(_("No nodes selected..."), ImVec2(0, 0), true);
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