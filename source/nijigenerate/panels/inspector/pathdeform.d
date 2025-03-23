module nijigenerate.panels.inspector.pathdeform;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijilive;
import std.format;
import std.utf;
import std.string;
import i18n;


/// Model View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: PathDeformer) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("PathDeformer"))) {
            float adjustSpeed = 1;

            igSpacing();

            // BLENDING MODE
            import std.conv : text;
            import std.string : toStringz;

            igSpacing();

            alias DefaultDriver = ConnectedPendulumDriver;

            if (ngCheckbox(__("Auto-Physics"), &physicsEnabled.value)) {
                physicsEnabled.apply();
                capture(cast(Node[])targets);
            }
            incTooltip(_("Enabled / Disabled physics driver for vertices. If enabled, vertices are moved along with specified physics engine."));

            string curveTypeText = curveType.value == CurveType.Bezier ? "Bezier" : curveType.value == CurveType.Spline? "Spline" : "Invalid";
            if (igBeginCombo("###PhysType", __(curveTypeText))) {

                if (igSelectable(__("Bezier"), curveType.value == CurveType.Bezier)) {
                    foreach (n; targets) {
                        n.curveType = CurveType.Bezier;
                        n.rebuffer(n.vertices);
                        n.notifyChange(n, NotifyReason.AttributeChanged);
                    }
                }

                if (igSelectable(__("Spline"), curveType.value == CurveType.Spline)) {
                    foreach (n; targets) {
                        n.curveType = CurveType.Spline;
                        n.rebuffer(n.vertices);
                        n.notifyChange(n, NotifyReason.AttributeChanged);
                    }
                }

                igEndCombo();
            }

            igSpacing();

            if (physicsEnabled.value) {
                incText(_("Physics Type"));
                string typeText = cast(ConnectedPendulumDriver)node.driver? "Pendulum": cast(ConnectedSpringPendulumDriver)node.driver? "SpringPendulum": "None";
                if (igBeginCombo("###PhysType", __(typeText))) {

                    if (igSelectable(__("Pendulum"), cast(ConnectedPendulumDriver)node.driver !is null)) {
                        if ((cast(ConnectedPendulumDriver)node.driver) is null) {
                            node.driver = new ConnectedPendulumDriver(node);
                        }
                        capture(cast(Node[])targets);
                    }

                    if (igSelectable(__("SpringPendulum"), cast(ConnectedSpringPendulumDriver)node.driver !is null)) {
                        if ((cast(ConnectedSpringPendulumDriver)node.driver) is null) {
                            node.driver = new ConnectedSpringPendulumDriver(node);
                        }
                        capture(cast(Node[])targets);
                    }

                    igEndCombo();
                }

                igSpacing();

                igPushID("PhysicsDriver");
                if (auto driver = cast(ConnectedPendulumDriver)node.driver) {

                    igPushID(0);
                        incText(_("Gravity scale"));
                        if(_shared!(gravity)(()=>incDragFloat("gravity", &gravity.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            gravity.apply();
                            foreach (n; targets)
                                n.notifyChange(n, NotifyReason.AttributeChanged);
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(1);
                        incText(_("Restore force"));
                        incTooltip(_("Force to restore for original position. If this force is weaker than the gravity, pendulum cannot restore to original position."));
                        if (_shared!restoreConstant(()=>incDragFloat("restoreConstant", &restoreConstant.value, adjustSpeed/100, 0.01, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            restoreConstant.apply();
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(2);
                        incText(_("Damping"));
                        if (_shared!damping(()=>incDragFloat("damping", &damping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            damping.apply();
                        }
                    igPopID();

                    igPushID(3);
                        incText(_("Input scale"));
                        incTooltip(_("Input force is multiplied by this factor. This should be specified when original position moved too much."));
                        if (_shared!inputScale(()=>incDragFloat("inputScale", &inputScale.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            inputScale.apply();
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(4);
                        incText(_("Propagate scale"));
                        incTooltip(_("Specify the degree to convey movement of previous pendulum to next one."));
                        if (_shared!propagateScale(()=>incDragFloat("propagateScale", &propagateScale.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            propagateScale.apply();
                        }
                    igPopID();                    
                    // Padding
                    igSpacing();
                    igSpacing();
                } else if (auto springPendulum = cast(ConnectedSpringPendulumDriver)node.driver) {

                }

                igPopID();
            }

        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(attribute!(bool, "physicsEnabled",   (x   )=>x~".physicsEnabled", (x, v)=>x~".physicsEnabled = "~v));
    mixin(attribute!(CurveType, "curveType",   (x)=>x~".curveType", (x, v)=>x~".curveType = "~v));
    mixin(attribute!(float, "gravity",         (x   )=>get_prop!float(_pendulum(x), "gravity"), 
                                               (x, v)=>set_prop!float(_pendulum(x), "gravity", "v") ));
    mixin(attribute!(float, "restoreConstant", (x   )=>get_prop!float(_pendulum(x), "restoreConstant"), 
                                               (x, v)=>set_prop!float(_pendulum(x), "restoreConstant", "v")));
    mixin(attribute!(float, "damping",         (x   )=>get_prop!float(_pendulum(x), "damping"), 
                                               (x, v)=>set_prop!float(_pendulum(x), "damping", "v")));
    mixin(attribute!(float, "inputScale",      (x   )=>get_prop!float(_pendulum(x), "inputScale"), 
                                               (x, v)=>set_prop!float(_pendulum(x), "inputScale", "v")));
    mixin(attribute!(float, "propagateScale",  (x   )=>get_prop!float(_pendulum(x), "propagateScale"), 
                                               (x, v)=>set_prop!float(_pendulum(x), "propagateScale", "v")));

    override
    void capture(Node[] targets) {
        super.capture(targets);
        physicsEnabled.capture();
        curveType.capture();
        gravity.capture();
        restoreConstant.capture();
        damping.capture();
        inputScale.capture();
        propagateScale.capture();
    }

private:
    static string _pendulum(string x) {
        return "() { if (auto d = cast(ConnectedPendulumDriver)("~x~".driver)) { return d; } else {return null;} }()";
    }
    static string get_prop(type)(string x, string prop) {
        return "(x) {return x? x."~prop~": "~type.stringof~".init; }("~x~")";
    }
    static string set_prop(type)(string x, string prop, string v) {
        return "if (auto x = "~x~") x."~prop~"="~v;
    }
}