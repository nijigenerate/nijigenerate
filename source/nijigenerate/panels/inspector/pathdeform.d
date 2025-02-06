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

private {

    void inspectDriver(T: ConnectedPendulumDriver)(T driver) {
        float adjustSpeed = 1;
        igPushID(0);
            incText(_("Gravity scale"));
            incDragFloat("gravity", &driver.gravity, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(1);
            incText(_("restore to original"));
            incDragFloat("restoreConstant", &driver.restoreConstant, adjustSpeed/100, 0.01, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(2);
            incText(_("Damping"));
            incDragFloat("damping", &driver.damping, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
        igPopID();

        igPushID(3);
            incText(_("Input scale"));
            incDragFloat("inputScale", &driver.inputScale, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
            igSpacing();
            igSpacing();
        igPopID();

        igPushID(4);
            incText(_("Propagate scale"));
            incDragFloat("propagateScale", &driver.propagateScale, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
        igPopID();
    }

}
/// Model View

void incInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: PathDeformer)(T node) {
    if (incBeginCategory(__("PathDeformer"))) {
        float adjustSpeed = 1;

        igSpacing();

        // BLENDING MODE
        import std.conv : text;
        import std.string : toStringz;

        igSpacing();

        alias DefaultDriver = ConnectedPendulumDriver;

        bool physicsEnabled = node.driver !is null;
        if (igCheckbox(__("Auto-Physics"), &physicsEnabled)) {
            if (physicsEnabled) {
                if (node.driver is null) {
                    node.driver = new DefaultDriver(node);
                }
            } else {
                if (node.driver !is null) {
                    node.driver = null;
                }
            }
        }
        incTooltip(_("Enabled / Disabled physics driver for vertices. If enabled, vertices are moved along with specified physics engine."));

        string curveTypeText = node.curveType == CurveType.Bezier ? "Bezier" : node.curveType == CurveType.Spline? "Spline" : "Invalid";
        if (igBeginCombo("###PhysType", __(curveTypeText))) {

            if (igSelectable(__("Bezier"), node.curveType == CurveType.Bezier)) {
                node.curveType = CurveType.Bezier;
                node.rebuffer(node.vertices);
            }

            if (igSelectable(__("Spline"), node.curveType == CurveType.Spline)) {
                node.curveType = CurveType.Spline;
                node.rebuffer(node.vertices);
            }

            igEndCombo();
        }

        igSpacing();

        if (physicsEnabled) {
            incText(_("Physics Type"));
            string typeText = cast(ConnectedPendulumDriver)node.driver? "Pendulum": cast(ConnectedSpringPendulumDriver)node.driver? "SpringPendulum": "None";
            if (igBeginCombo("###PhysType", __(typeText))) {

                if (igSelectable(__("Pendulum"), cast(ConnectedPendulumDriver)node.driver !is null)) {
                    if ((cast(ConnectedPendulumDriver)node.driver) is null) {
                        node.driver = new ConnectedPendulumDriver(node);
                    }
                }

                if (igSelectable(__("SpringPendulum"), cast(ConnectedSpringPendulumDriver)node.driver !is null)) {
                    if ((cast(ConnectedSpringPendulumDriver)node.driver) is null) {
                        node.driver = new ConnectedSpringPendulumDriver(node);
                    }
                }

                igEndCombo();
            }

            igSpacing();

            igPushID("PhysicsDriver");
            if (auto driver = cast(ConnectedPendulumDriver)node.driver) {
                inspectDriver(driver);
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

/// Armed Parameter View

void incInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: PathDeformer)(T node, Parameter param, vec2u cursor) {
    if (incBeginCategory(__("Path Deformer"))) {
        float adjustSpeed = 1;
        igPushID("PathDeformer");

        bool physicsEnabled = node.driver !is null;
        if (physicsEnabled) {
            igSpacing();

            igPushID("PhysicsDriver");
            if (auto driver = cast(ConnectedPendulumDriver)node.driver) {
                inspectDriver(driver);
            } else if (auto springPendulum = cast(ConnectedSpringPendulumDriver)node.driver) {

            }

            igPopID();
        }

        // Padding
        igSpacing();
        igSpacing();

        igPopID();
    }
    incEndCategory();
}