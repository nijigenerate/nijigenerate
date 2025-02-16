module nijigenerate.panels.inspector.common;

import nijigenerate.viewport.vertex;
import nijigenerate.viewport.model.deform;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate;
import nijilive;
import i18n;
import std.format;
import std.algorithm;
import std.algorithm.searching;
import std.string;
import std.stdio;
import std.utf;
import std.array;

package(nijigenerate.panels.inspector) {
    ImVec4 CategoryTextColor = ImVec4(0.36f, 0.45f, 0.35f, 1.00f); // 画像の緑色に基づく
}

/// Model View.

interface Inspector(T) {
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init);
    void capture(T[] nodes);
    bool acceptable(T[] nodes);
    ModelEditSubMode subMode();
    void subMode(ModelEditSubMode);
}
class BaseInspector(ModelEditSubMode targetMode: ModelEditSubMode.Layout, T: Node) : Inspector!Node {
protected:
    T[] targets;
    ModelEditSubMode mode;
public:
    this(T[] t, ModelEditSubMode mode) {
        capture(cast(Node[])t);
        this.mode = mode;
    }
    override
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init) {
        if (mode == targetMode)
            run();
    }
    abstract void run();
    override
    void capture(Node[] nodes) { 
        targets.length = 0;
        foreach (n; nodes) {
            if (auto t = cast(T)n)
                targets ~= t;
        }
    }
    override
    bool acceptable(Node[] nodes) {
        return nodes.all!((node) => cast(T)node !is null);
    }

    override
    ModelEditSubMode subMode() { return mode; }

    override
    void subMode(ModelEditSubMode value) { mode = value; }
}

class BaseInspector(ModelEditSubMode targetMode: ModelEditSubMode.Deform, T: Node) : Inspector!Node {
protected:
    T[] targets;
    ModelEditSubMode mode;
public:
    this(T[] t, ModelEditSubMode mode) {
        capture(cast(Node[])t);
        this.mode = mode;
    }
    override
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init) {
        if (mode == targetMode)
            run(parameter, cursor);
    }
    abstract void run(Parameter parameter, vec2u cursor);
    override
    void capture(Node[] nodes) {
        targets.length = 0;
        foreach (n; nodes) {
            if (auto t = cast(T)n)
                targets ~= t;
        }        
    }
    override
    bool acceptable(Node[] nodes) {
        return nodes.all!((node) => cast(T)node !is null);
    }

    override
    ModelEditSubMode subMode() { return mode; }

    override
    void subMode(ModelEditSubMode value) { mode = value; }
}

class InspectorHolder(T) : Inspector!T {
protected:
    Inspector!T[] inspectors;
    T[] targets;
    ModelEditSubMode mode;

public:
    this(T[] targets, ModelEditSubMode mode) {
        capture(cast(T[])targets);
        this.mode = mode;        
    }
    override
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init) {
        auto mode = ngModelEditSubMode();
        if (targets.length == 1) {
            if (mode == ModelEditSubMode.Layout) {
                static if (is(T: Node)) {
                    incModelModeHeader(targets.length > 0 ? targets[0]: null);
                }
            } else if (mode == ModelEditSubMode.Deform) {
                static if (is(T: Node)) {
                    incCommonNonEditHeader(targets.length > 0 ? targets[0]: null);
                }
            }
        } else if (targets.length > 1) {
            ngMultiEditHeader(cast(Node[])targets);
        }

        foreach (i; inspectors) {
            i.subMode = mode;
            if (i.acceptable(targets))
                i.inspect(parameter, cursor);
        }
    }

    void setInspectors(Inspector!T[] inspectors) {
        this.inspectors = inspectors;
        foreach (i; inspectors) { 
            i.capture(targets);
        }
    }

    override
    void capture(T[] nodes) {
        targets.length = 0;
        foreach (n; nodes) {
            if (auto t = cast(T)n)
                targets ~= t;
        }
        foreach (i; inspectors) { 
            i.capture(targets);
        }
    }

    override
    bool acceptable(T[] nodes) {
        return inspectors.all!((t) => t.acceptable(nodes));
    }

    ModelEditSubMode subMode() { return mode; }
    void subMode(ModelEditSubMode value) { mode = value; }

    T[] getTargets() { return targets; }
}

void incModelModeHeader(Node node) {
    // Top level
    igPushID(node.uuid);
        string typeString = "%s".format(incTypeIdToIcon(node.typeId()));
        auto len = incMeasureString(typeString);
        if (incInputText("###MODEL_NODE_HEADER", incAvailableSpace().x-24, node.name_)) {
            try {
                node.name_ = node.name.toStringz.fromStringz;
            } catch (std.utf.UTFException e) {}
        }
        igSameLine(0, 0);
        incDummy(ImVec2(-len.x, len.y));
        igSameLine(0, 0);
        incText(typeString);
    igPopID();
}

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

void ngMultiEditHeader(Node[] nodes) {
    // Top level
    igPushID(nodes[0].uuid);
        string typeString = nodes.map!((n) => incTypeIdToIcon(n.typeId())).array.join("");
        auto len = incMeasureString(typeString);
        incText(nodes.map!((n) => n.name).array.join(","));
        igSameLine(0, 0);
        incDummy(ImVec2(-len.x, len.y));
        igSameLine(0, 0);
        incText(typeString);
    igPopID();
    igSeparator();
}

/// Deformation View.

void incInspectorDeformFloatDragVal(string name, string paramName, float adjustSpeed, Node node, Parameter param, vec2u cursor, bool rotation=false) {
    float currFloat = incInspectorDeformGetValue(node, param, paramName, cursor);

    if (ngInspectorDeformFloatDragVal(name, &currFloat, adjustSpeed, rotation)) {
        incInspectorDeformSetValue(node, param, paramName, cursor, currFloat);
    }
}

bool ngInspectorDeformFloatDragVal(string name, float* result, float adjustSpeed, bool rotation=false) {
    // Convert to degrees for display
    float currFloat;
    if (rotation) { currFloat = degrees(*result); } else { currFloat = *result; }

    if (incDragFloat(name, &currFloat, adjustSpeed, -float.max, float.max, rotation ? "%.2f°" : "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {        
        // Convert back to radians for data managment
        if (rotation) {
            *result = radians(currFloat);
        } else {
            *result = currFloat;
        }
        return true;
    }
    return false;
}
void incInspectorDeformInputFloat(string name, string paramName, float step, float stepFast, Node node, Parameter param, vec2u cursor) {
    float currFloat = incInspectorDeformGetValue(node, param, paramName, cursor);
    if (ngInspectorDeformInputFloat(name, &currFloat, step, stepFast)) {
        incInspectorDeformSetValue(node, param, paramName, cursor, currFloat);
    }
}

bool ngInspectorDeformInputFloat(string name, float* result, float step, float stepFast) {
    return igInputFloat(name.toStringz, result, step, stepFast, "%.2f");
}

void incInspectorDeformColorEdit3(string[3] paramNames, Node node, Parameter param, vec2u cursor) {
    import std.math : isNaN;
    float[3] rgb = [float.nan, float.nan, float.nan];
    float[3] rgbadj = [1, 1, 1];
    bool[3] rgbchange = [false, false, false];
    ValueParameterBinding pbr = cast(ValueParameterBinding)param.getBinding(node, paramNames[0]);
    ValueParameterBinding pbg = cast(ValueParameterBinding)param.getBinding(node, paramNames[1]);
    ValueParameterBinding pbb = cast(ValueParameterBinding)param.getBinding(node, paramNames[2]);

    if (pbr) {
        rgb[0] = pbr.getValue(cursor);
        rgbadj[0] = rgb[0];
    }

    if (pbg) {
        rgb[1] = pbg.getValue(cursor);
        rgbadj[1] = rgb[1];
    }

    if (pbb) {
        rgb[2] = pbb.getValue(cursor);
        rgbadj[2] = rgb[2];
    }

    if (igColorEdit3("###COLORADJ", &rgbadj)) {

        // RED
        if (rgbadj[0] != 1) {
            auto b = cast(ValueParameterBinding)param.getOrAddBinding(node, paramNames[0]);
            b.setValue(cursor, rgbadj[0]);
        } else if (pbr) {
            pbr.setValue(cursor, rgbadj[0]);
        }

        // GREEN
        if (rgbadj[1] != 1) {
            auto b = cast(ValueParameterBinding)param.getOrAddBinding(node, paramNames[1]);
            b.setValue(cursor, rgbadj[1]);
        } else if (pbg) {
            pbg.setValue(cursor, rgbadj[1]);
        }

        // BLUE
        if (rgbadj[2] != 1) {
            auto b = cast(ValueParameterBinding)param.getOrAddBinding(node, paramNames[2]);
            b.setValue(cursor, rgbadj[2]);
        } else if (pbb) {
            pbb.setValue(cursor, rgbadj[2]);
        }
    }
}

void incInspectorDeformSliderFloat(string name, string paramName, float min, float max, Node node, Parameter param, vec2u cursor) {
    float currFloat = incInspectorDeformGetValue(node, param, paramName, cursor);
    if (ngInspectorDeformSliderFloat(name, &currFloat, min, max)) {
        incInspectorDeformSetValue(node, param, paramName, cursor, currFloat);
    }
}

bool ngInspectorDeformSliderFloat(string name, float* result, float min, float max) {
    return (igSliderFloat(name.toStringz, result, min, max, "%.2f"));
}

void incInspectorDeformDragFloat(string name, string paramName, float speed, float min, float max, const(char)* fmt, Node node, Parameter param, vec2u cursor) {
    float value = incInspectorDeformGetValue(node, param, paramName, cursor);
    if (ngInspectorDeformDragFloat(name, &value, speed, min, max, fmt)) {
        incInspectorDeformSetValue(node, param, paramName, cursor, value);
    }
}

bool ngInspectorDeformDragFloat(string name, float* result, float speed, float min, float max, const(char)* fmt) {
    return igDragFloat(name.toStringz, result, speed, min, max, fmt);
}

float incInspectorDeformGetValue(Node node, Parameter param, string paramName, vec2u cursor) {
    float currFloat = node.getDefaultValue(paramName);
    if (ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName)) {
        currFloat = b.getValue(cursor);
    }
    return currFloat;
}

void incInspectorDeformSetValue(Node node, Parameter param, string paramName, vec2u cursor, float value) {
    GroupAction groupAction = null;
    ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName);
    if (b is null) {
        b = cast(ValueParameterBinding)param.createBinding(node, paramName);
        param.addBinding(b);
        groupAction = new GroupAction();
        auto addAction = new ParameterBindingAddAction(param, b);
        groupAction.addAction(addAction);
    }
    auto action = new ParameterBindingValueChangeAction!(float, ValueParameterBinding)(b.getName(), b, cursor.x, cursor.y);
    b.setValue(cursor, value);
    action.updateNewState();
    if (groupAction) {
        groupAction.addAction(action);
        incActionPush(groupAction);
    } else {
        incActionPush(action);
    }

    if (auto editor = incViewportModelDeformGetEditor()) {
        if (auto e = editor.getEditorFor(node)) {
            e.adjustPathTransform();
        }
    }
}

void incInspectorDeformSetValue(T)(T[] nodes, Parameter param, string paramName, vec2u cursor, float value) {
    GroupAction groupAction = null;
    ValueParameterBinding[] bs = nodes.map!((node) {
        auto b = cast(ValueParameterBinding)param.getBinding(node, paramName);
        if (b is null) {
            b = cast(ValueParameterBinding)param.createBinding(node, paramName);
            param.addBinding(b);
            groupAction = new GroupAction();
            auto addAction = new ParameterBindingAddAction(param, b);
            groupAction.addAction(addAction);
        }
        return b;
    }).array;
    auto action = new ParameterBindingValueChangeAction!(float, ValueParameterBinding[])(bs[0].getName(), bs, cursor.x, cursor.y);
    foreach (b; bs)
        b.setValue(cursor, value);
    action.updateNewState();
    if (groupAction) {
        groupAction.addAction(action);
        incActionPush(groupAction);
    } else {
        incActionPush(action);
    }

    if (auto editor = incViewportModelDeformGetEditor()) {
        foreach (n; nodes) {
            if (auto e = editor.getEditorFor(n)) {
                e.adjustPathTransform();
            }
        }
    }
}
mixin template MultiEdit() {

    mixin template SharedValue(T2) {
        bool isShared;
        T2 value;
    }

    Parameter currParam = null;
    vec2u currCursor = vec2u.init;

    bool _shared(alias varName)(bool delegate() editFunc) {
        if (targets.length == 1) return editFunc();
        bool valueChanged = false;

        igBeginGroup();
        float width = igCalcItemWidth();

        if (!varName.isShared) {
            // **varName.isShared が false の場合はエディットボックスを使わず、ComboBox を表示**
            igSetNextItemWidth(width);
            if (igBeginCombo(("##combo_" ~ __traits(identifier, varName)).toStringz, "Select Value")) {
                foreach (t; targets) {
                    typeof(varName.value) value = varName.get(t);

                    // "オブジェクト名: 値" の形式で表示
                    string displayValue = "%s: %s".format(t.name, value);

                    if (igSelectable(displayValue.toStringz, false)) {
                        varName.value = value;
                        valueChanged = true;
                        break;
                    }
                }
                igEndCombo();
            }
        } else {
            // **varName.isShared が true の場合は通常のエディットボックスを表示**
            igSetNextItemWidth(width);
            valueChanged |= editFunc();
        }

        igEndGroup();
        return valueChanged;
    }

    void updateDeform(Parameter param, vec2u cursor) {
        if (currParam != param || currCursor != cursor) {
            currParam = param;
            currCursor = cursor;
            capture(cast(Node[])targets);
        }
    }

    void _deform(alias varName)(bool delegate(string, typeof(&varName.value)) editFunc) {
        if (_shared!varName(() => editFunc("###"~__traits(identifier, varName), &varName.value))) {
            varName.apply();
        }
    }

    static string attribute(type, alias name, string function(string) _getter = null, string function(string, string) _setter = null)() {
        string getter(string x) { return _getter? _getter(x): (x~"."~name); }
        string setter(string x, string v) { return _setter? _setter(x, v): (x~"."~name~"="~v); }
        enum result =  
        "class SharedValue_"~name~" {
            "~typeof(this).stringof~" parent;
            mixin SharedValue!("~type.stringof~");
            this("~typeof(this).stringof~" parent) {
                this.parent = parent;
            }
            "~type.stringof~" get(T n) {
                return "~getter("n")~";
            }
            void set(T n,"~type.stringof~" v) {
                "~setter("n", "v")~";
                this.isShared = true;
            }
            bool capture() {
                if (targets.length == 0) {
                    this.isShared = false;
                    return false;
                }
                this.isShared = true;
                this.value    = this.get(parent.targets[0]);
                foreach (n; parent.targets[1..$]) {
                    if (this.value != this.get(n)) {this.isShared = false; return false; }
                }
                return true;
            }
            void apply() {
                foreach (n; parent.targets) {
                    this.set(n,this.value);
                }
            }

        }
        SharedValue_"~name~" _"~name~";
        SharedValue_"~name~" "~name~"() {
            if (_"~name~" is null) _"~name~" = new SharedValue_"~name~"(this);
            return _"~name~";
        }
        ";
//        pragma(msg, "attribute:\n", result);
        return result;
    }


    static string deformation(alias name, string propName = null)() {
        string getter(string x) { return "incInspectorDeformGetValue("~x~",parent.currParam,\""~(propName?propName:name)~"\", parent.currCursor)"; }
        string setter(string x, string v) { return "incInspectorDeformSetValue("~x~", parent.currParam,\""~(propName?propName:name)~"\", parent.currCursor,"~v~")"; }
        enum result = "
        class SharedValue_"~name~" {
            "~typeof(this).stringof~" parent;
            mixin SharedValue!float;
            this("~typeof(this).stringof~" parent) {
                this.parent = parent;
            }
            float get(T n) {
                return "~getter("n")~";
            }
            void set(T[] n,float v) {
                "~setter("n", "v")~";
                this.isShared = true;
            }
            bool capture() {
                if (currParam is null) { return false; }
                if (targets.length == 0) {
                    this.isShared = false;
                    return false;
                }
                this.isShared = true;
                this.value    = this.get(parent.targets[0]);
                foreach (n; parent.targets[1..$]) {
                    if (this.value != this.get(n)) {this.isShared = false; return false; }
                }
                return true;
            }
            void apply() {
                if (currParam is null) return;
                this.set(parent.targets, this.value);
            }
        }
        SharedValue_"~name~" _"~name~" = null;
        SharedValue_"~name~" "~name~"() {
            if (_"~name~" is null) _"~name~" = new SharedValue_"~name~"(this);
            return _"~name~";
        }
        ";
//        pragma(msg, "deformation:\n", result);
        return result;
    }

}