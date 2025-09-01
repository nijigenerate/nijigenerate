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
import std.algorithm : sort;
import std.array : array;
import nijigenerate.commands; // for cmd!
import nijigenerate.commands.parameter.prop : ParamPropCommand;

// Reuse types used by axes editor
import nijigenerate.widgets.controller : EditableAxisPoint, incControllerAxisDemo;

enum ParamEditorTab { Properties, Axes }

class ParamEditorWindow : Window {
private:
    Parameter param;
    ParamEditorTab defaultTab; // kept for ctor compatibility; no tabs in UI

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
        // Preserve original order; append midpoint of the largest gap in display order
        import std.range : iota;
        size_t n = points[axis].length;
        size_t[] order = iota(n).array;
        sort!((a, b) => points[axis][a].value < points[axis][b].value)(order);
        float newValue;
        if (n >= 2) {
            size_t bestK = 0;
            float bestGap = -float.infinity;
            foreach (k; 0 .. n - 1) {
                if (k + 1 >= n) break;
                float lv = points[axis][order[k]].value;
                float rv = points[axis][order[k + 1]].value;
                float gap = rv - lv;
                if (gap > bestGap) { bestGap = gap; bestK = k; }
            }
            float lv2 = points[axis][order[bestK]].value;
            float rv2 = points[axis][order[bestK + 1]].value;
            newValue = (lv2 + rv2) * 0.5f;
        } else if (n == 1) {
            newValue = (axis == 0 ? (min.x + max.x) : (min.y + max.y)) * 0.5f;
        } else {
            newValue = 0;
        }
        float newNorm = mapAxisLocal(cast(uint)axis, newValue);
        points[axis] ~= EditableAxisPoint(-1, false, newValue, newNorm);
        this.findEndPoint();
    }

    void axisPointList(ulong axis, ImVec2 avail) {
        int deleteIndex = -1;
        igIndent();
        igPushID(cast(int)axis);
        if (igBeginChild("###AXIS_ADJ", ImVec2(0, avail.y))) {
            if (points[axis].length > 2) {
                import std.range : iota;
                size_t[] order = iota(points[axis].length).array;
                sort!((a, b) => points[axis][a].value < points[axis][b].value)(order);
                foreach(pos, idx; order) {
                    auto ref pt = points[axis][idx];
                    if (pt.fixed) continue;
                    vec2 range;
                    if (pt.origIndex != -1) {
                        float leftV = (pos > 0) ? points[axis][order[pos-1]].value : (axis==0? min.x : min.y);
                        float rightV = (pos+1 < order.length) ? points[axis][order[pos+1]].value : (axis==0? max.x : max.y);
                        range = vec2(leftV, rightV);
                    } else if (axis == 0) {
                        range = vec2(min.x, max.x);
                    } else {
                        range = vec2(min.y, max.y);
                    }
                    range = range + vec2(0.01, -0.01);
                    igSetNextItemWidth(80);
                    igPushID(cast(int)idx);
                    if (incDragFloat("adj_offset", &pt.value, 0.01, range.x, range.y, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                        pt.normValue = mapAxisLocal(cast(uint)axis, pt.value);
                    }
                    igSameLine(0, 0);
                    if (idx == endPoint.vector[axis]) {
                        incDummy(ImVec2(-52, 32));
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)idx;
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) createPoint(axis);
                    } else {
                        incDummy(ImVec2(-28, 32));
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)idx;
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
        // Main editor content (leave space for bottom buttons)
        if (igBeginChild("###ParamEditorContent", ImVec2(0, -36))) {
            // Properties (inline)
            incText(_("Parameter Name"));
            igIndent();
                incInputText("Name", paramName);
            igUnindent();

            // If constraints changed, re-sync point values to new range (keep normalized positions)
            static vec2 prevMinCache; static vec2 prevMaxCache; static bool initDone;
            if (!initDone) { prevMinCache = min; prevMaxCache = max; initDone = true; }
            if (prevMinCache != min || prevMaxCache != max) {
                foreach(ax; 0..points.length) foreach(ref pt; points[ax]) pt.value = unmapAxisLocal(cast(uint)ax, pt.normValue);
                findEndPoint();
                prevMinCache = min; prevMaxCache = max;
            }

            // Axes editing section (no categories)
                ImVec2 avail = incAvailableSpace();
                float reqSpace = param.isVec2 ? 128 : 32;
                if (igBeginChild("###ControllerView", ImVec2(192, avail.y))) {
                    incDummy(ImVec2(0, (avail.y/2)-(reqSpace/2)));
                    incControllerAxisDemo("###CONTROLLER", param, points, ImVec2(192, reqSpace));
                }
                igEndChild();
                igSameLine(0,0);
                igBeginGroup();
                if (igBeginChild("###ControllerSettings", ImVec2(0, 0))) {
                    avail = incAvailableSpace();
                    if (param.isVec2) {
                        float colW = (avail.x - 8); // spacing accounted after first column
                        colW *= 0.5f;

                        // X column
                        if (igBeginChild("###AxisX", ImVec2(colW, 0))) {
                            incText("X");
                            igIndent();
                                igSetNextItemWidth(64);
                                igPushID(0);
                                    incDragFloat("adj_x_min", &min.vector[0], 1, -float.max, max.x-1, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                                igPopID();
                                igSameLine(0, 4);
                                igSetNextItemWidth(64);
                                igPushID(1);
                                    incDragFloat("adj_x_max", &max.vector[0], 1, min.x+1, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                                igPopID();
                            igUnindent();
                            auto axAvail = incAvailableSpace();
                            axisPointList(0, ImVec2(axAvail.x, axAvail.y));
                        }
                        igEndChild();

                        igSameLine(0, 8);

                        // Y column
                        if (igBeginChild("###AxisY", ImVec2(0, 0))) {
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
                            auto ayAvail = incAvailableSpace();
                            axisPointList(1, ImVec2(ayAvail.x, ayAvail.y));
                        }
                        igEndChild();
                    } else {
                        // 1D: min/max + list (single column)
                        incText(_("Breakpoints"));
                        igIndent();
                            igSetNextItemWidth(64);
                            igPushID(0);
                                incDragFloat("adj_x_min", &min.vector[0], 1, -float.max, max.x-1, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                            igPopID();
                            igSameLine(0, 4);
                            igSetNextItemWidth(64);
                            igPushID(1);
                                incDragFloat("adj_x_max", &max.vector[0], 1, min.x+1, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                            igPopID();
                        igUnindent();
                        auto a1 = incAvailableSpace();
                        axisPointList(0, ImVec2(a1.x, a1.y));
                    }
                }
                igEndChild();
                igEndGroup();
            
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
                    foreach(x; 0..points[axis].length) foreach(xi; 0..points[axis].length) {
                        if (x == xi) continue;
                        if (points[axis][x].normValue == points[axis][xi].normValue) {
                            incDialog(__("Error"), _("One or more axes points are overlapping, this is not allowed."));
                            success = false; break iloop;
                        }
                    }
                }
                if (success) {
                    // Apply via commands
                    if (!isValidName()) {
                        incDialog(__("Error"), _("Name is already taken"));
                    } else {
                        Context ctx = new Context();
                        ctx.puppet = incActivePuppet();
                        ctx.parameters = [param];

                        // Name only if changed
                        if (param.name != paramName) {
                            cmd!(ParamPropCommand.SetParameterName)(ctx, paramName);
                        }

                        // Collect normalized breakpoints and apply props+axes
                        float[] axisX; foreach (pt; points[0]) axisX ~= pt.normValue; axisX.sort();
                        float[] axisY; if (param.isVec2) { foreach (pt; points[1]) axisY ~= pt.normValue; axisY.sort(); }

                        float[2] minArr = [min.x, min.y];
                        float[2] maxArr = [max.x, max.y];
                        if (param.isVec2)
                            cmd!(ParamPropCommand.ApplyParameterPropsAxes)(ctx, minArr, maxArr, axisX, axisY);
                        else
                            cmd!(ParamPropCommand.ApplyParameterPropsAxes)(ctx, minArr, maxArr, axisX, new float[](0));
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
