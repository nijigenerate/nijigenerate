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

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: SimplePhysics) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
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
            if (_shared!modelType(() {
                    bool result = false;
                    if (igBeginCombo("###PhysType", __(modelType.value.text))) {
                        if (igSelectable(__("Pendulum"), modelType.value == PhysicsModel.Pendulum)) {
                            modelType.value = PhysicsModel.Pendulum;
                            result = true;
                        }

                        if (igSelectable(__("SpringPendulum"), modelType.value == PhysicsModel.SpringPendulum)) {
                            modelType.value = PhysicsModel.SpringPendulum;
                            result = true;
                        }
                        igEndCombo();
                    }
                    return result;
            })) {
                apply_modelType();
            }

            igSpacing();

            incText(_("Mapping mode"));
            if (_shared!mapMode(() {
                    bool result = false;
                    if (igBeginCombo("###PhysMapMode", __(mapMode.value.text))) {
                        if (igSelectable(__("AngleLength"), mapMode.value == ParamMapMode.AngleLength)) {
                            mapMode.value = ParamMapMode.AngleLength;
                            result = true;
                        }

                        if (igSelectable(__("XY"), mapMode.value == ParamMapMode.XY)) {
                            mapMode.value = ParamMapMode.XY;
                            result = true;
                        }

                        if (igSelectable(__("LengthAngle"), mapMode.value == ParamMapMode.LengthAngle)) {
                            mapMode.value = ParamMapMode.LengthAngle;
                            result = true;
                        }

                        if (igSelectable(__("YX"), mapMode.value == ParamMapMode.YX)) {
                            mapMode.value = ParamMapMode.YX;
                            result = true;
                        }
                        igEndCombo();
                    }
                    return result;
            })) {
                apply_mapMode();
            }

            igSpacing();

            igPushID("SimplePhysics");
            
            igPushID(-1);
                if (_shared!localOnly(()=>ngCheckbox(__("Local Transform Lock"), &localOnly.value))) {
                    apply_localOnly();
                };
                incTooltip(_("Whether the physics system only listens to the movement of the physics node itself"));
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(0);
                incText(_("Gravity scale"));
                if (_shared!gravity(()=>incDragFloat("gravity", &gravity.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_gravity();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(1);
                incText(_("Length"));
                if (_shared!length(()=>incDragFloat("length", &length.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_length();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(2);
                incText(_("Resonant frequency"));
                if (_shared!frequency(()=>incDragFloat("frequency", &frequency.value, adjustSpeed/100, 0.01, 30, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_frequency();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(3);
                incText(_("Damping"));
                if (_shared!angleDamping(()=>incDragFloat("damping_angle", &angleDamping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_angleDamping();
                }
            igPopID();

            igPushID(4);
                if (_shared!lengthDamping(()=>incDragFloat("damping_length", &lengthDamping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_lengthDamping();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(5);
                incText(_("Output scale"));
                if (_shared!(outputScaleX, "outputScale.x")(()=>incDragFloat("output_scale.x", &outputScaleX.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_outputScaleX();
                }
            igPopID();

            igPushID(6);
                if (_shared!(outputScaleY, "outputScale.y")(()=>incDragFloat("output_scale.y", &outputScaleY.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    apply_outputScaleY();
                }
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

    mixin MultiEdit;
    mixin(attribute!(PhysicsModel, "modelType"));
    mixin(attribute!(ParamMapMode, "mapMode"));
    mixin(attribute!(bool, "localOnly"));
    mixin(attribute!(float, "gravity"));
    mixin(attribute!(float, "length"));
    mixin(attribute!(float, "frequency"));
    mixin(attribute!(float, "angleDamping"));
    mixin(attribute!(float, "lengthDamping"));
    mixin(attribute!(float, "outputScaleX", (x)=>x~".outputScale.vector[0]", (x, v)=>x~".outputScale.vector[0]="~v));
    mixin(attribute!(float, "outputScaleY", (x)=>x~".outputScale.vector[1]", (x, v)=>x~".outputScale.vector[1]="~v));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        capture_localOnly();
        capture_modelType();
        capture_mapMode();
        capture_gravity();
        capture_length();
        capture_frequency();
        capture_angleDamping();
        capture_lengthDamping();
        capture_outputScaleX();
        capture_outputScaleY();
    }
}
/// Armed Parameter View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: SimplePhysics) : BaseInspector!(mode, T) {
public:
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run(Parameter param, vec2u cursor) {
        if (targets.length == 0) return;
        auto node = targets[0];

        if (currParam != param || currCursor != cursor) {
            currParam = param;
            currCursor = cursor;
            capture(cast(Node[])targets);
        }

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

    mixin MultiEdit;

    mixin(deformation!"gravity");
    mixin(deformation!"length");
    mixin(deformation!"frequency");
    mixin(deformation!"angleDamping");
    mixin(deformation!"lengthDamping");
    mixin(deformation!("outputScaleX","outputScale.x"));
    mixin(deformation!("outputScaleY","outputScale.y"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        capture_gravity();
        capture_length();
        capture_frequency();
        capture_angleDamping();
        capture_lengthDamping();
        capture_outputScaleX();
        capture_outputScaleY();
    }
}