module nijigenerate.panels.inspector.simplephysics;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.panels.parameters;
import nijilive;
import std.format;
import std.utf;
import std.string;
import i18n;

/// Model View

void incInspectorModelSimplePhysics(SimplePhysics node) {
    if (incBeginCategory(__("SimplePhysics"))) {
        float adjustSpeed = 1;

        igSpacing();

        // BLENDING MODE
        import std.conv : text;
        import std.string : toStringz;

        igPushID("TargetParam");
            if (igBeginPopup("TPARAM")) {
                if (node.param) {
                    if (igMenuItem(__("Unmap"))) {
                        node.param = null;
                        incActivePuppet().rescanNodes();
                    }
                } else {
                    incDummyLabel(_("Unassigned"), ImVec2(128, 16));
                }

                igEndPopup();
            }

            incText(_("Parameter"));
            string paramName = _("(unassigned)");
            if (node.param !is null) paramName = node.param.name;
            igInputText("###TARGET_PARAM", cast(char*)paramName.toStringz, paramName.length, ImGuiInputTextFlags.ReadOnly);
            igOpenPopupOnItemClick("TPARAM", ImGuiPopupFlags.MouseButtonRight);

            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PARAMETER");
                if (payload !is null) {
                    ParamDragDropData* payloadParam = *cast(ParamDragDropData**)payload.Data;
                    node.param = payloadParam.param;
                    incActivePuppet().rescanNodes();
                }

                igEndDragDropTarget();
            }

        igPopID();

        incText(_("Type"));
        if (igBeginCombo("###PhysType", __(node.modelType.text))) {

            if (igSelectable(__("Pendulum"), node.modelType == PhysicsModel.Pendulum)) node.modelType = PhysicsModel.Pendulum;

            if (igSelectable(__("SpringPendulum"), node.modelType == PhysicsModel.SpringPendulum)) node.modelType = PhysicsModel.SpringPendulum;

            igEndCombo();
        }

        igSpacing();

        incText(_("Mapping mode"));
        if (igBeginCombo("###PhysMapMode", __(node.mapMode.text))) {

            if (igSelectable(__("AngleLength"), node.mapMode == ParamMapMode.AngleLength)) node.mapMode = ParamMapMode.AngleLength;

            if (igSelectable(__("XY"), node.mapMode == ParamMapMode.XY)) node.mapMode = ParamMapMode.XY;

            if (igSelectable(__("LengthAngle"), node.mapMode == ParamMapMode.LengthAngle)) node.mapMode = ParamMapMode.LengthAngle;

            if (igSelectable(__("YX"), node.mapMode == ParamMapMode.YX)) node.mapMode = ParamMapMode.YX;

            igEndCombo();
        }

        igSpacing();

        igPushID("SimplePhysics");
        
        igPushID(-1);
            igCheckbox(__("Local Transform Lock"), &node.localOnly);
            incTooltip(_("Whether the physics system only listens to the movement of the physics node itself"));
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(0);
            incText(_("Gravity scale"));
            incDragFloat("gravity", &node.gravity, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(1);
            incText(_("Length"));
            incDragFloat("length", &node.length, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(2);
            incText(_("Resonant frequency"));
            incDragFloat("frequency", &node.frequency, adjustSpeed/100, 0.01, 30, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(3);
            incText(_("Damping"));
            incDragFloat("damping_angle", &node.angleDamping, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
        igPopID();

        igPushID(4);
            incDragFloat("damping_length", &node.lengthDamping, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(5);
            incText(_("Output scale"));
            incDragFloat("output_scale.x", &node.outputScale.vector[0], adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
        igPopID();

        igPushID(6);
            incDragFloat("output_scale.y", &node.outputScale.vector[1], adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        // Padding
        igSpacing();
        igSpacing();

        igPopID();
        }
    incEndCategory();
}

/// Armed Parameter View

void incInspectorDeformSimplePhysics(SimplePhysics node, Parameter param, vec2u cursor) {
    if (incBeginCategory(__("Simple Physics"))) {
        float adjustSpeed = 1;
        igPushID("SimplePhysics");

            igPushID(0);
                incText(_("Gravity scale"));
                incInspectorDeformDragFloat("###Gravity", "gravity", adjustSpeed/100, -float.max, float.max, "%.2f", node, param, cursor);
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(1);
                incText(_("Length"));
                incInspectorDeformDragFloat("###Length", "length", adjustSpeed/100, 0, float.max, "%.2f", node, param, cursor);
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(2);
                incText(_("Resonant frequency"));
                incInspectorDeformDragFloat("###ResFreq", "frequency", adjustSpeed/100, 0.01, 30, "%.2f", node, param, cursor);
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(3);
                incText(_("Damping"));
                incInspectorDeformDragFloat("###AngleDamp", "angleDamping", adjustSpeed/100, 0, 5, "%.2f", node, param, cursor);
            igPopID();

            igPushID(4);
                incInspectorDeformDragFloat("###Length", "lengthDamping", adjustSpeed/100, 0, 5, "%.2f", node, param, cursor);
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(5);
                incText(_("Output scale"));
                incInspectorDeformDragFloat("###OutScaleX", "outputScale.x", adjustSpeed/100, 0, float.max, "%.2f", node, param, cursor);
            igPopID();

            igPushID(6);
                incInspectorDeformDragFloat("###OutScaleY", "outputScale.y", adjustSpeed/100, 0, float.max, "%.2f", node, param, cursor);
                igSpacing();
                igSpacing();
            igPopID();

            // Padding
            igSpacing();
            igSpacing();

        igPopID();
    }
    incEndCategory();
}