/*
    Combined Parameter Editor Window (Properties + Axes)
*/
module nijigenerate.windows.parameditor;

import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate;
import std.string;
import nijigenerate.utils.link;
import i18n;
import nijilive;
import std.math;

// Reuse types used by axes editor
import nijigenerate.widgets.controller : EditableAxisPoint, incControllerAxisDemo;

enum ParamEditorTab { Properties, Axes }

class ParamEditorWindow : Window {
private:
    Parameter param;
    ParamEditorTab defaultTab;

    // Properties state
    string paramName;
    vec2 min;
    vec2 max;

    bool isValidName() {
        auto ex = cast(ExPuppet)incActivePuppet();
        if (ex is null) return true;
        Parameter fparam = ex.findParameter(paramName);
        return fparam is null || fparam.uuid == param.uuid;
    }

    // Axes state
    EditableAxisPoint[][2] points;
    vec2 endPoint;

    // Local mapping based on edited min/max values (not yet applied to param)
    float mapAxisLocal(uint axis, float value) {
        float aMin = axis == 0 ? min.x : min.y;
        float aMax = axis == 0 ? max.x : max.y;
        if (aMax == aMin) return 0; // avoid div by zero
        return (value - aMin) / (aMax - aMin);
    }
    float unmapAxisLocal(uint axis, float norm) {
        float aMin = axis == 0 ? min.x : min.y;
        float aMax = axis == 0 ? max.x : max.y;
        return aMin + norm * (aMax - aMin);
    }

    void findEndPoint() {
        foreach(i, x; points[0]) {
            if (!x.fixed) endPoint.x = i;
        }
        foreach(i, y; points[1]) {
            if (!y.fixed) endPoint.y = i;
        }
    }

    void createPoint(ulong axis) {
        float normValue = (points[axis][0].normValue + points[axis][1].normValue) / 2;
        float value = unmapAxisLocal(cast(uint)axis, normValue);
        points[axis] ~= EditableAxisPoint(-1, false, value, normValue);
        this.findEndPoint();
    }

    void axisPointList(ulong axis, ImVec2 avail) {
        int deleteIndex = -1;
        igIndent();
        igPushID(cast(int)axis);
        if (igBeginChild("###AXIS_ADJ", ImVec2(0, avail.y))) {
            if (points[axis].length > 2) {
                int ix;
                foreach(i, ref pt; points[axis]) {
                    ix++;
                    if (pt.fixed) continue;
                    vec2 range;
                    if (pt.origIndex != -1) {
                        range = vec2(points[axis][i - 1].value, points[axis][i + 1].value);
                    } else if (axis == 0) {
                        range = vec2(min.x, max.x);
                    } else {
                        range = vec2(min.y, max.y);
                    }
                    range = range + vec2(0.01, -0.01);
                    igSetNextItemWidth(80);
                    igPushID(cast(int)i);
                    if (incDragFloat("adj_offset", &pt.value, 0.01, range.x, range.y, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                        pt.normValue = mapAxisLocal(cast(uint)axis, pt.value);
                    }
                    igSameLine(0, 0);
                    if (i == endPoint.vector[axis]) {
                        incDummy(ImVec2(-52, 32));
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)i;
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) createPoint(axis);
                    } else {
                        incDummy(ImVec2(-28, 32));
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)i;
                    }
                    igPopID();
                }
            } else {
                incDummy(ImVec2(-28, 32));
                igSameLine(0, 0);
                if (incButtonColored("", ImVec2(24, 24))) createPoint(axis);
            }
        }
        igEndChild();
        igPopID();
        igUnindent();
        if (deleteIndex != -1) {
            import std.algorithm.mutation : remove;
            points[axis] = points[axis].remove(cast(uint)deleteIndex);
            this.findEndPoint();
        }
    }

protected:
    override void onBeginUpdate() {
        igSetNextWindowSize(ImVec2(384*2, 192*2), ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(ImVec2(384, 192), ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override void onUpdate() {
        igPushID(cast(void*)param);
        // Upper area for tabs (leave space for bottom buttons)
        if (igBeginChild("###ParamEditorTabs", ImVec2(0, -36))) {
            if (igBeginTabBar("###ParamEditorTabBar", ImGuiTabBarFlags.None)) {
                void renderProps() {
                    if (igBeginTabItem(__("Properties"))) {
                    if (igBeginChild("###MainSettings", ImVec2(0, -28))) {
                        incText(_("Parameter Name"));
                        igIndent();
                        incInputText("Name", paramName);
                        igUnindent();

                        incText(_("Parameter Constraints"));
                        igIndent();
                        igSetNextItemWidth(256);
                        if (param.isVec2) incText("X");
                        if (param.isVec2) igIndent();
                        igSetNextItemWidth(64);
                        igPushID(0);
                        incDragFloat("adj_x_min", &min.vector[0], 1, -float.max, max.x-1, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                        igPopID();
                        igSameLine(0, 4);
                        igSetNextItemWidth(64);
                        igPushID(1);
                        incDragFloat("adj_x_max", &max.vector[0], 1, min.x+1, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                        igPopID();
                        if (param.isVec2) igUnindent();
                        if (param.isVec2) {
                            incText("Y");
                            igIndent();
                            igSetNextItemWidth(64);
                            igPushID(2);
                            incDragFloat("adj_y_min", &min.vector[1], 1, -float.max, max.y-1, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                            igPopID();
                            igSameLine(0, 4);
                            igSetNextItemWidth(64);
                            igPushID(3);
                            incDragFloat("adj_y_max", &max.vector[1], 1, min.y+1, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                            igPopID();
                            igUnindent();
                        }
                        igUnindent();
                    }
                    igEndChild();
                        igEndTabItem();
                    }
                }
                void renderAxes() {
                    if (igBeginTabItem(__("Axes"))) {
                    ImVec2 avail = incAvailableSpace();
                    float reqSpace = param.isVec2 ? 128 : 32;
                    if (igBeginChild("###ControllerView", ImVec2(192, avail.y))) {
                        incDummy(ImVec2(0, (avail.y/2)-(reqSpace/2)));
                        incControllerAxisDemo("###CONTROLLER", param, points, ImVec2(192, reqSpace));
                    }
                    igEndChild();
                    igSameLine(0,0);
                    igBeginGroup();
                    if (igBeginChild("###ControllerSettings", ImVec2(0, -(28)))) {
                        avail = incAvailableSpace();
                        if (param.isVec2) {
                            if (incBeginCategory("X", IncCategoryFlags.NoCollapse)) axisPointList(0, ImVec2(avail.x, (avail.y/2)-42));
                            incEndCategory();
                            if (incBeginCategory("Y", IncCategoryFlags.NoCollapse)) axisPointList(1, ImVec2(avail.x, (avail.y/2)-42));
                            incEndCategory();
                        } else {
                            if (incBeginCategory(__("Breakpoints"), IncCategoryFlags.NoCollapse)) axisPointList(0, ImVec2(avail.x, avail.y-38));
                            incEndCategory();
                        }
                    }
                    igEndChild();
                    igEndGroup();
                        igEndTabItem();
                    }
                }
                // Reverse tab order: Axes first, then Properties
                renderProps();
                renderAxes();
                igEndTabBar();
            }
        }
        igEndChild();

        // Bottom button bar
        if (igBeginChild("###SettingsBtns", ImVec2(0, 0))) {
            incDummy(ImVec2(-132, 0));
            igSameLine(0, 0);
            if (incButtonColored(__("Cancel"), ImVec2(64, 24))) {
                this.close();
            }
            igSameLine(0, 4);
            if (incButtonColored(__("Save"), ImVec2(64, 24))) {
                // Validate axes (no overlap)
                bool success = true;
                iloop: foreach(axis; 0..points.length) {
                    foreach(x; 0..points[0].length) foreach(xi; 0..points[0].length) {
                        if (x == xi) continue;
                        if (points[axis][x].normValue == points[axis][xi].normValue) {
                            incDialog(__("Error"), _("One or more axes points are overlapping, this is not allowed."));
                            success = false; break iloop;
                        }
                    }
                }
                if (success) {
                    // Apply properties
                    if (!isValidName) {
                        incDialog(__("Error"), _("Name is already taken"));
                    } else {
                        param.name = paramName;
                        param.makeIndexable();
                        auto prevMin = param.min; auto prevMax = param.max;
                        param.min = min; param.max = max;
                        if (prevMin.x != param.min.x) incActionPush(new ParameterValueChangeAction!float("min X", param, incGetDragFloatInitialValue("adj_x_min"), param.min.vector[0], &param.min.vector[0]));
                        if (prevMin.y != param.min.y) incActionPush(new ParameterValueChangeAction!float("min Y", param, incGetDragFloatInitialValue("adj_y_min"), param.min.vector[1], &param.min.vector[1]));
                        if (prevMax.x != param.max.x) incActionPush(new ParameterValueChangeAction!float("max X", param, incGetDragFloatInitialValue("adj_x_max"), param.max.vector[0], &param.max.vector[0]));
                        if (prevMax.y != param.max.y) incActionPush(new ParameterValueChangeAction!float("max Y", param, incGetDragFloatInitialValue("adj_y_max"), param.max.vector[1], &param.max.vector[1]));

                        // Apply axes
                        foreach (axis, axisPoints; points) {
                            int skew = 0;
                            foreach (i, ref point; axisPoints) {
                                if (point.origIndex != -1) {
                                    while (point.origIndex != -1 && (i + skew) < point.origIndex) {
                                        param.deleteAxisPoint(cast(uint)axis, cast(uint)i);
                                        skew++;
                                    }
                                    if (!point.fixed)
                                        param.axisPoints[axis][i] = point.normValue;
                                } else {
                                    param.insertAxisPoint(cast(uint)axis, point.normValue);
                                }
                            }
                        }
                        this.close();
                    }
                }
            }
        }
        igEndChild();
        igPopID();
    }

public:
    this(ref Parameter param, ParamEditorTab tab = ParamEditorTab.Properties) {
        this.param = param;
        this.defaultTab = tab;
        // Init properties state
        paramName = param.name.dup;
        min.vector = param.min.vector.dup;
        max.vector = param.max.vector.dup;
        // Init axes state
        foreach(i, ref axisPoints; points) {
            axisPoints.length = param.axisPoints[i].length;
            foreach(j, ref point; axisPoints) {
                point.origIndex = cast(int)j;
                point.normValue = param.axisPoints[i][j];
                point.value = unmapAxisLocal(cast(uint)i, point.normValue);
            }
            axisPoints[0].fixed = true;
            axisPoints[$ - 1].fixed = true;
        }
        this.findEndPoint();

        super(_("Parameter Editor"));
    }
}
