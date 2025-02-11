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
import std.algorithm.searching;
import std.string;
import std.stdio;
import std.utf;

package(nijigenerate.panels.inspector) {
    ImVec4 CategoryTextColor = ImVec4(0.36f, 0.45f, 0.35f, 1.00f); // 画像の緑色に基づく
}

/// Model View.

interface Inspector(T) {
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init);
    void capture(T[] nodes);
    bool acceptable(T node);
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
    bool acceptable(Node node) {
        return cast(T)node !is null;
    }

    override
    ModelEditSubMode subMode() { return mode; }

    override
    void subMode(ModelEditSubMode value) { mode = value; }
}

class BaseInspector(ModelEditSubMode targetMode: ModelEditSubMode.Layout, T: Puppet) : Inspector!Puppet {
protected:
    T[] targets;
    ModelEditSubMode mode;
public:
    this(T[] t, ModelEditSubMode mode) {
        capture(cast(Puppet[])t);
        this.mode = mode;
    }
    override
    void inspect(Parameter parameter = null, vec2u cursor = vec2u.init) {
        if (mode == targetMode)
            run();
    }
    abstract void run();
    override
    void capture(Puppet[] nodes) { }
    override
    bool acceptable(Puppet node) {
        return cast(T)node !is null;
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
    bool acceptable(Node node) {
        return cast(T)node !is null;
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
        if (mode == ModelEditSubMode.Layout) {
            static if (is(T: Node)) {
                incModelModeHeader(targets.length > 0 ? targets[0]: null);
            }
        } else if (mode == ModelEditSubMode.Deform) {
            static if (is(T: Node)) {
                incCommonNonEditHeader(targets.length > 0 ? targets[0]: null);
            }
        }

        foreach (i; inspectors) {
            i.subMode = mode;
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
    bool acceptable(T node) {
        return inspectors.any!((t) => t.acceptable(node));
    }

    ModelEditSubMode subMode() { return mode; }
    void subMode(ModelEditSubMode value) { mode = value; }
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

/// Deformation View.

void incInspectorDeformFloatDragVal(string name, string paramName, float adjustSpeed, Node node, Parameter param, vec2u cursor, bool rotation=false) {
    float currFloat = node.getDefaultValue(paramName);
    if (ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName)) {
        currFloat = b.getValue(cursor);
    }

    // Convert to degrees for display
    if (rotation) currFloat = degrees(currFloat);

    if (incDragFloat(name, &currFloat, adjustSpeed, -float.max, float.max, rotation ? "%.2f°" : "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
        
        // Convert back to radians for data managment
        if (rotation) currFloat = radians(currFloat);

        // Set binding
        GroupAction groupAction = null;
        ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName);
        if (b is null) {
            b = cast(ValueParameterBinding)param.createBinding(node, paramName);
            param.addBinding(b);
            groupAction = new GroupAction();
            auto addAction = new ParameterBindingAddAction(param, b);
            groupAction.addAction(addAction);
        }

        // Push action
        auto action = new ParameterBindingValueChangeAction!(float)(b.getName(), b, cursor.x, cursor.y);
        b.setValue(cursor, currFloat);
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
}

void incInspectorDeformInputFloat(string name, string paramName, float step, float stepFast, Node node, Parameter param, vec2u cursor) {
    float currFloat = node.getDefaultValue(paramName);
    if (ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName)) {
        currFloat = b.getValue(cursor);
    }
    if (igInputFloat(name.toStringz, &currFloat, step, stepFast, "%.2f")) {
        GroupAction groupAction = null;
        ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName);
        if (b is null) {
            b = cast(ValueParameterBinding)param.createBinding(node, paramName);
            param.addBinding(b);
            groupAction = new GroupAction();
            auto addAction = new ParameterBindingAddAction(param, b);
            groupAction.addAction(addAction);
        }
        auto action = new ParameterBindingValueChangeAction!(float)(b.getName(), b, cursor.x, cursor.y);
        b.setValue(cursor, currFloat);
        action.updateNewState();
        if (groupAction) {
            groupAction.addAction(action);
            incActionPush(groupAction);
        } else {
            incActionPush(action);
        }
    }
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
    float currFloat = node.getDefaultValue(paramName);
    if (ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName)) {
        currFloat = b.getValue(cursor);
    }
    if (igSliderFloat(name.toStringz, &currFloat, min, max, "%.2f")) {
        GroupAction groupAction = null;
        ValueParameterBinding b = cast(ValueParameterBinding)param.getBinding(node, paramName);
        if (b is null) {
            b = cast(ValueParameterBinding)param.createBinding(node, paramName);
            param.addBinding(b);
            groupAction = new GroupAction();
            auto addAction = new ParameterBindingAddAction(param, b);
            groupAction.addAction(addAction);
        }
        auto action = new ParameterBindingValueChangeAction!(float)(b.getName(), b, cursor.x, cursor.y);
        b.setValue(cursor, currFloat);
        action.updateNewState();
        if (groupAction) {
            groupAction.addAction(action);
            incActionPush(groupAction);
        } else {
            incActionPush(action);
        }
    }
}

void incInspectorDeformDragFloat(string name, string paramName, float speed, float min, float max, const(char)* fmt, Node node, Parameter param, vec2u cursor) {
    float value = incInspectorDeformGetValue(node, param, paramName, cursor);
    if (igDragFloat(name.toStringz, &value, speed, min, max, fmt)) {
        incInspectorDeformSetValue(node, param, paramName, cursor, value);
    }
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
        auto action = new ParameterBindingValueChangeAction!(float)(b.getName(), b, cursor.x, cursor.y);
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
