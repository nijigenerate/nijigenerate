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
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijilive;
import i18n;
import std.utf;
import std.string;

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
                            incInspectorDeformTRS(node, param, cursor);

                            // Node Part Section
                            if (Part part = cast(Part)node) {
                                incInspectorDeformPart(part, param, cursor);
                            }

                            if (Composite composite = cast(Composite)node) {
                                incInspectorDeformComposite(composite, param, cursor);
                            }

                            if (SimplePhysics phys = cast(SimplePhysics)node) {
                                incInspectorDeformSimplePhysics(phys, param, cursor);
                            }

                        } else {
                            incModelModeHeader(node);
                            incInspectorModelTRS(node);

                            // Node Camera Section
                            if (ExCamera camera = cast(ExCamera)node) {
                                incInspectorModelCamera(camera);
                            }

                            // Node Drawable Section
                            if (Composite composite = cast(Composite)node) {
                                incInspectorModelComposite(composite);
                            }


                            // Node Drawable Section
                            if (Drawable drawable = cast(Drawable)node) {
                                incInspectorModelDrawable(drawable);
                            }

                            // Node Part Section
                            if (Part part = cast(Part)node) {
                                incInspectorModelPart(part);
                            }

                            // Node SimplePhysics Section
                            if (SimplePhysics part = cast(SimplePhysics)node) {
                                incInspectorModelSimplePhysics(part);
                            }

                            // Node MeshGroup Section
                            if (MeshGroup group = cast(MeshGroup)node) {
                                incInspectorModelMeshGroup(group);
                            }
                        }
                    
                    break;
                    default:
                        incCommonNonEditHeader(node);
                        break;
                }
            } else incInspectorModelInfo();
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
