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

            if (targets.length == 1) {
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
            }

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
                }
            )) {
                modelType.apply();
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
                mapMode.apply();
            }

            igSpacing();

            igPushID("SimplePhysics");
            
            igPushID(-1);
                if (_shared!localOnly(()=>ngCheckbox(__("Local Transform Lock"), &localOnly.value))) {
                    localOnly.apply();
                }
                incTooltip(_("Whether the physics system only listens to the movement of the physics node itself"));
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(0);
                incText(_("Gravity scale"));
                if (_shared!gravity(()=>incDragFloat("gravity", &gravity.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    gravity.apply();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(1);
                incText(_("Length"));
                if (_shared!length(()=>incDragFloat("length", &length.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    length.apply();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(2);
                incText(_("Resonant frequency"));
                if (_shared!frequency(()=>incDragFloat("frequency", &frequency.value, adjustSpeed/100, 0.01, 30, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    frequency.apply();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(3);
                incText(_("Damping"));
                if (_shared!angleDamping(()=>incDragFloat("damping_angle", &angleDamping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    angleDamping.apply();
                }
            igPopID();

            igPushID(4);
                if (_shared!lengthDamping(()=>incDragFloat("damping_length", &lengthDamping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    lengthDamping.apply();
                }
                igSpacing();
                igSpacing();
            igPopID();

            igPushID(5);
                incText(_("Output scale"));
                if (_shared!(outputScaleX)(()=>incDragFloat("output_scale.x", &outputScaleX.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    outputScaleX.apply();
                }
            igPopID();

            igPushID(6);
                if (_shared!(outputScaleY)(()=>incDragFloat("output_scale.y", &outputScaleY.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    outputScaleY.apply();
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
        localOnly.capture();
        modelType.capture();
        mapMode.capture();
        gravity.capture();
        length.capture();
        frequency.capture();
        angleDamping.capture();
        lengthDamping.capture();
        outputScaleX.capture();
        outputScaleY.capture();
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

        updateDeform(param, cursor);

        if (incBeginCategory(__("Simple Physics"))) {
            float adjustSpeed = 1;
            igPushID("SimplePhysics");

                igPushID(0);
                    incText(_("Gravity scale"));
                    _deform!gravity((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                    igSpacing();
                    igSpacing();
                igPopID();

                igPushID(1);
                    incText(_("Length"));
                    _deform!length((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                    igSpacing();
                    igSpacing();
                igPopID();

                igPushID(2);
                    incText(_("Resonant frequency"));
                    _deform!frequency((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                    igSpacing();
                    igSpacing();
                igPopID();

                igPushID(3);
                    incText(_("Damping"));
                    _deform!angleDamping((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                igPopID();

                igPushID(4);
                    _deform!lengthDamping((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                    igSpacing();
                    igSpacing();
                igPopID();

                igPushID(5);
                    incText(_("Output scale"));
                    _deform!outputScaleX((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
                igPopID();

                igPushID(6);
                    _deform!outputScaleY((s, v)=>ngInspectorDeformDragFloat(s, v, adjustSpeed/100, -float.max, float.max, "%.2f"));
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
        gravity.capture();
        length.capture();
        frequency.capture();
        angleDamping.capture();
        lengthDamping.capture();
        outputScaleX.capture();
        outputScaleY.capture();
    }
}