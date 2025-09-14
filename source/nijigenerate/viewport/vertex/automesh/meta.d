module nijigenerate.viewport.vertex.automesh.meta;

// Minimal scaffolding for AutoMesh reflection

enum AutoMeshLevel { Preset, Simple, Advanced }

struct AMParam {
    AutoMeshLevel level;
    string id;
    string label;
    string desc;
    string widget;
    float min = float.nan;
    float max = float.nan;
    float step = float.nan;
    int order = 0;
}
struct AMArray { float min = float.nan; float max = float.nan; float step = float.nan; }
struct AMEnum { string[] values; }
struct AMPreset { string name; }
struct AMPresetProvider {}
// Class-level metadata for processor identity and order.
struct AMProcessor { string id; string displayName; int order = 0; }

interface IAutoMeshReflect {
    string schema();
    string values(string levelName);
    bool   applyPreset(string name);
    bool   writeValues(string levelName, string updatesJson);
}

// Generic reflection/mixin to unify AutoMesh processors
// Usage inside a processor class: `mixin AutoMeshReflection();`
// Requirements on the class:
//  - Fields annotated with @AMParam (float or float[]). For float[], optionally add @AMArray for min/max/step
//  - Optional static preset functions annotated with @AMPreset("Name"): `static void presetX(This p) { ... }`
//  - Optional AlphaPreview state field named `_alphaPreview` of type AlphaPreviewState; if present, preview UI is shown
//  - Optional hook: `void ngPostParamWrite(string id)` is called when a parameter is changed/applied
import std.json : JSONValue, JSONType;
static import std.json;
alias parseJSON = std.json.parseJSON;
import i18n;
import bindbc.imgui;
import nijigenerate.widgets : incDragFloat, incDummy, incButtonColored, incText, incBeginCategory, incEndCategory, incDummyLabel;
import nijigenerate.widgets.category : IncCategoryFlags;
import nijigenerate.viewport.vertex.automesh.alpha_provider : AlphaPreviewState, alphaPreviewWidget;

mixin template AutoMeshReflection() {
    alias This = typeof(this);

    private struct _AMFloatField {
        string id; string label; AutoMeshLevel level; string widget; float minV; float maxV; float stepV; float* ptr;
    }
    private struct _AMArrayField {
        string id; string label; AutoMeshLevel level; string widget; float minV; float maxV; float stepV; float[]* ptr;
    }
    private struct _AMEnumField {
        string id; string label; AutoMeshLevel level; string[] values; string* ptr;
    }
    private _AMFloatField[] _amFloatFields() {
        _AMFloatField[] outp;
        static foreach (mname; __traits(allMembers, This)) {{
            static if (__traits(compiles, __traits(getMember, this, mname))) {
                static if (is(typeof(__traits(getMember, this, mname)) == float)) {
                    string id, label, widget; AutoMeshLevel level;
                    float minV = float.nan, maxV = float.nan, step = float.nan;
                    foreach (attr; __traits(getAttributes, __traits(getMember, This, mname))) {
                        static if (is(typeof(attr) == AMParam)) {
                            id = attr.id; label = attr.label; widget = attr.widget; level = attr.level;
                            minV = attr.min; maxV = attr.max; step = attr.step;
                        }
                    }
                    if (id.length) outp ~= _AMFloatField(id, label, level, widget, minV, maxV, step, &mixin("this." ~ mname));
                }
            }
        }}
        return outp;
    }
    private _AMArrayField[] _amArrayFields() {
        _AMArrayField[] outp;
        static foreach (mname; __traits(allMembers, This)) {{
            static if (__traits(compiles, __traits(getMember, this, mname))) {
                static if (is(typeof(__traits(getMember, this, mname)) == float[])) {
                    string id, label, widget; AutoMeshLevel level;
                    float minV = float.nan, maxV = float.nan, step = float.nan;
                    foreach (attr; __traits(getAttributes, __traits(getMember, This, mname))) {
                        static if (is(typeof(attr) == AMParam)) { id = attr.id; label = attr.label; widget = attr.widget; level = attr.level; }
                        static if (is(typeof(attr) == AMArray)) { minV = attr.min; maxV = attr.max; step = attr.step; }
                    }
                    if (id.length) outp ~= _AMArrayField(id, label, level, widget, minV, maxV, step, &mixin("this." ~ mname));
                }
            }
        }}
        return outp;
    }

    private _AMEnumField[] _amEnumFields() {
        _AMEnumField[] outp;
        static foreach (mname; __traits(allMembers, This)) {{
            static if (__traits(compiles, __traits(getMember, this, mname))) {
                static if (is(typeof(__traits(getMember, this, mname)) == string)) {
                    string id, label; AutoMeshLevel level; string[] choices;
                    foreach (attr; __traits(getAttributes, __traits(getMember, This, mname))) {
                        static if (is(typeof(attr) == AMParam)) { id = attr.id; label = attr.label; level = attr.level; }
                        static if (is(typeof(attr) == AMEnum)) { choices = attr.values; }
                    }
                    if (id.length && choices.length) outp ~= _AMEnumField(id, label, level, choices, &mixin("this." ~ mname));
                }
            }
        }}
        return outp;
    }

    private string[] _amPresetNames() {
        string[] names;
        static foreach (mname; __traits(allMembers, This)) {
            static if (__traits(compiles, __traits(getOverloads, This, mname))) {
                foreach (ovl; __traits(getOverloads, This, mname)) {
                    foreach (attr; __traits(getAttributes, ovl)) {
                        static if (is(typeof(attr) == AMPreset)) names ~= attr.name;
                    }
                }
            }
        }
        return names;
    }

    // IAutoMeshReflect
    string schema() {
        JSONValue obj = JSONValue(JSONType.object);
        // type name after last dot
        auto tn = typeid(cast(Object)this).toString();
        size_t lastDot = 0; bool hasDot = false; foreach (i, ch; tn) if (ch == '.') { lastDot = i; hasDot = true; }
        obj["type"] = hasDot ? tn[lastDot + 1 .. $] : tn;
        // presets
        JSONValue presets = JSONValue(JSONType.array);
        foreach (n; _amPresetNames()) { JSONValue p; p["name"] = n; presets.array ~= p; }
        obj["presets"] = presets;
        // params
        JSONValue simple = JSONValue(JSONType.array);
        JSONValue advanced = JSONValue(JSONType.array);
        foreach (f; _amFloatFields()) {
            JSONValue it; it["id"] = f.id; it["label"] = f.label; it["type"] = "float";
            if ((f.minV == f.minV)) it["min"] = JSONValue(cast(double)f.minV);
            if ((f.maxV == f.maxV)) it["max"] = JSONValue(cast(double)f.maxV);
            if ((f.stepV == f.stepV)) it["step"] = JSONValue(cast(double)f.stepV);
            (f.level == AutoMeshLevel.Simple ? simple : advanced).array ~= it;
        }
        foreach (a; _amArrayFields()) {
            JSONValue it; it["id"] = a.id; it["label"] = a.label; it["type"] = "float[]";
            (a.level == AutoMeshLevel.Simple ? simple : advanced).array ~= it;
        }
        foreach (e; _amEnumFields()) {
            JSONValue it; it["id"] = e.id; it["label"] = e.label; it["type"] = "enum";
            JSONValue vals = JSONValue(JSONType.array); foreach (v; e.values) { JSONValue sv; sv = v; vals.array ~= sv; } it["values"] = vals;
            (e.level == AutoMeshLevel.Simple ? simple : advanced).array ~= it;
        }
        obj["Simple"] = simple; obj["Advanced"] = advanced;
        return obj.toString();
    }
    string values(string levelName) {
        bool adv = levelName == "Advanced";
        JSONValue v = JSONValue(JSONType.object);
        foreach (f; _amFloatFields()) if (((f.level == AutoMeshLevel.Advanced) == adv)) v[f.id] = JSONValue(cast(double)(*f.ptr));
        foreach (a; _amArrayFields()) if (((a.level == AutoMeshLevel.Advanced) == adv)) {
            JSONValue arr = JSONValue(JSONType.array); foreach (x; *a.ptr) arr.array ~= JSONValue(cast(double)x); v[a.id] = arr;
        }
        foreach (e; _amEnumFields()) if (((e.level == AutoMeshLevel.Advanced) == adv)) {
            v[e.id] = (*e.ptr).length ? (*e.ptr) : "";
        }
        return v.toString();
    }
    bool applyPreset(string name) {
        bool applied = false;
        static foreach (mname; __traits(allMembers, This)) {
            static if (__traits(compiles, __traits(getOverloads, This, mname))) {
                foreach (ovl; __traits(getOverloads, This, mname)) {
                    foreach (attr; __traits(getAttributes, ovl)) {
                        static if (is(typeof(attr) == AMPreset)) {
                            if (!applied && name == attr.name) { mixin("This." ~ mname ~ "(this);"); applied = true; }
                        }
                    }
                }
            }
        }
        return applied;
    }
    bool writeValues(string levelName, string updatesJson) {
        auto u = parseJSON(updatesJson);
        if (u.type != JSONType.object) return false;
        bool adv = levelName == "Advanced";
        bool any = false;
        foreach (ref f; _amFloatFields()) if (((f.level == AutoMeshLevel.Advanced) == adv) && (f.id in u)) {
            auto j = u[f.id];
            if (j.type == JSONType.integer) { *f.ptr = cast(float)j.integer; any = true; }
            else if (j.type == JSONType.float_) { *f.ptr = cast(float)j.floating; any = true; }
            if ((f.minV == f.minV) && *f.ptr < f.minV) *f.ptr = f.minV;
            if ((f.maxV == f.maxV) && *f.ptr > f.maxV) *f.ptr = f.maxV;
            static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(f.id);
        }
        foreach (ref a; _amArrayFields()) if (((a.level == AutoMeshLevel.Advanced) == adv) && (a.id in u)) {
            auto j = u[a.id]; if (j.type != JSONType.array) continue; (*a.ptr).length = 0;
            foreach (e; j.array) {
                if (e.type == JSONType.integer) (*a.ptr) ~= cast(float)e.integer;
                else if (e.type == JSONType.float_) (*a.ptr) ~= cast(float)e.floating;
            }
            any = true;
            static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(a.id);
        }
        foreach (ref e; _amEnumFields()) if (((e.level == AutoMeshLevel.Advanced) == adv) && (e.id in u)) {
            auto j = u[e.id];
            if (j.type == JSONType.string) {
                auto cand = j.str;
                // Validate against choices
                foreach (v; e.values) if (v == cand) { *e.ptr = cand; any = true; break; }
            }
            static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(e.id);
        }
        return any;
    }

    // Unified options UI
    override void configure() {
        import std.conv : to;
        string _uidPrefix() {
            auto addr = cast(size_t) cast(void*) this;
            return procId() ~ "@" ~ addr.to!string;
        }
        string _wuid(string sub) { return _uidPrefix() ~ "/" ~ sub; }
        igPushID((_uidPrefix() ~ ":CONFIGURE_OPTIONS\0").ptr);
        // Presets if available (render as Combo inline, not as category)
        const hasPresets = _amPresetNames().length > 0;
        if (hasPresets) {
            incText(_("Presets"));
            igIndent();
                auto presetList = _amPresetNames();
                string currentPreset;
                static if (__traits(hasMember, This, "presetName")) {
                    // Initialize default preset if empty
                    if (!presetName.length) {
                        string def = "Normal parts";
                        bool found = false;
                        foreach (n; presetList) if (n == def) { presetName = def; found = true; break; }
                        if (!found && presetList.length) presetName = presetList[0];
                    }
                    currentPreset = presetName;
                } else {
                    currentPreset = presetList.length ? presetList[0] : "";
                }
                igPushID((_wuid("PRESET_COMBO") ~ '\0').ptr);
                if (igBeginCombo((_wuid("PRESETS") ~ '\0').ptr, __(currentPreset))) {
                    foreach (name; presetList) {
                        if (igSelectable(__(name))) {
                            applyPreset(name);
                            static if (__traits(hasMember, This, "presetName")) presetName = name;
                        }
                    }
                    igEndCombo();
                }
                igPopID();
            igUnindent();
        }

        {
            string _slabel = _("Simple");
            string _sid = _slabel ~ "###" ~ _wuid("CAT_SIMPLE");
            bool _open = incBeginCategory((_sid ~ '\0').ptr);
            if (_open) {
            foreach (ref f; _amFloatFields()) if (f.level == AutoMeshLevel.Simple) {
                incText(_(f.label)); igIndent();
                igSetNextItemWidth(96);
                string fmt = f.stepV == 1 ? "%.0f" : "%.2f";
                igPushID((_wuid(f.id) ~ '\0').ptr);
                bool changed = incDragFloat(_wuid(f.id), f.ptr, (f.stepV==f.stepV)?f.stepV:1, (f.minV==f.minV)?f.minV:0, (f.maxV==f.maxV)?f.maxV:0, fmt, ImGuiSliderFlags.NoRoundToFormat);
                igPopID();
                igUnindent();
                if (changed) { static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(f.id); }
            }
            // Enums (Simple)
            foreach (ref e; _amEnumFields()) if (e.level == AutoMeshLevel.Simple) {
                incText(_(e.label)); igIndent();
                auto cur = (*e.ptr).length ? (*e.ptr) : "";
                auto curZ = (cur ~ '\0').ptr;
                auto idZ = (_wuid(e.id) ~ '\0').ptr;
                if (igBeginCombo(idZ, curZ)) {
                    foreach (v; e.values) {
                        auto vz = (v ~ '\0').ptr;
                        if (igSelectable(vz)) { *e.ptr = v; static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(e.id); }
                    }
                    igEndCombo();
                }
                igUnindent();
            }
            }
            incEndCategory();
        }
        {
            string _alabel = _("Advanced");
            string _aid = _alabel ~ "###" ~ _wuid("CAT_ADVANCED");
            bool _open = incBeginCategory((_aid ~ '\0').ptr, IncCategoryFlags.DefaultClosed);
            if (_open) {
            foreach (ref f; _amFloatFields()) if (f.level == AutoMeshLevel.Advanced) {
                incText(_(f.label)); igIndent();
                igSetNextItemWidth(96);
                string fmt = f.stepV == 1 ? "%.0f" : "%.2f";
                igPushID((_wuid(f.id) ~ '\0').ptr);
                bool changed = incDragFloat(_wuid(f.id), f.ptr, (f.stepV==f.stepV)?f.stepV:1, (f.minV==f.minV)?f.minV:0, (f.maxV==f.maxV)?f.maxV:0, fmt, ImGuiSliderFlags.NoRoundToFormat);
                igPopID();
                igUnindent();
                if (changed) { static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(f.id); }
            }
            // Enums (Advanced)
            foreach (ref e; _amEnumFields()) if (e.level == AutoMeshLevel.Advanced) {
                incText(_(e.label)); igIndent();
                auto cur = (*e.ptr).length ? (*e.ptr) : "";
                auto curZ = (cur ~ '\0').ptr;
                auto idZ2 = (_wuid(e.id) ~ '\0').ptr;
                if (igBeginCombo(idZ2, curZ)) {
                    foreach (v; e.values) {
                        auto vz = (v ~ '\0').ptr;
                        if (igSelectable(vz)) { *e.ptr = v; static if (__traits(hasMember, This, "ngPostParamWrite")) this.ngPostParamWrite(e.id); }
                    }
                    igEndCombo();
                }
                igUnindent();
            }
            foreach (ref a; _amArrayFields()) {
                int deleteIndex = -1;
                incText(_(a.label)); igIndent();
                igPushID((_wuid(a.id) ~ '\0').ptr);
                if ((*a.ptr).length) {
                    foreach (i, _; *a.ptr) {
                        igSetNextItemWidth(96);
                        igPushID(cast(int)i);
                            incDragFloat("value", &((*a.ptr)[i]), (a.stepV==a.stepV)?a.stepV:0.01, (a.minV==a.minV)?a.minV:-1, (a.maxV==a.maxV)?a.maxV:2, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                            igSameLine(0, 0);
                            if (i == (*a.ptr).length - 1) {
                                incDummy(ImVec2(-52, 32)); igSameLine(0, 0);
                                if (incButtonColored("\uE92E", ImVec2(24, 24))) deleteIndex = cast(int)i;
                                igSameLine(0, 0);
                                if (incButtonColored("\uE145", ImVec2(24, 24))) (*a.ptr) ~= 1.0;
                            } else {
                                incDummy(ImVec2(-28, 32)); igSameLine(0, 0);
                                if (incButtonColored("\uE92E", ImVec2(24, 24))) deleteIndex = cast(int)i;
                            }
                        igPopID();
                    }
                } else {
                    incDummy(ImVec2(-28, 24)); igSameLine(0, 0);
                    if (incButtonColored("\uE145", ImVec2(24, 24))) (*a.ptr) ~= 1.0;
                }
                if (deleteIndex != -1) (*a.ptr) = (*a.ptr).remove(cast(uint)deleteIndex);
                igPopID();
                igUnindent();
            }
            }
            incEndCategory();
        }
        igPopID();

        // Alpha preview when available on class
        static if (__traits(hasMember, This, "_alphaPreview")) {
            igSeparator();
            incText(_("Alpha Preview"));
            igIndent();
            alphaPreviewWidget(_alphaPreview, ImVec2(192, 192));
            igUnindent();
        }
    }
}

// Class-level identity overrides via UDA
mixin template AutoMeshClassInfo() {
    alias T = typeof(this);
    private static string _fallbackId() {
        // Use unqualified identifier when available
        return __traits(identifier, T);
    }
    override string procId() {
        string id;
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                id = attr.id.length ? attr.id : _fallbackId();
            }
        }
        return id.length ? id : _fallbackId();
    }
    override string displayName() {
        string name;
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                name = attr.displayName.length ? attr.displayName : null;
            }
        }
        return name.length ? name : procId();
    }
    override int order() {
        int ord = int.min;
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                ord = attr.order;
            }
        }
        return ord == int.min ? 0 : ord;
    }
}

// Compile-time processor info from UDA
template AMProcInfo(alias T) {
    enum string id = ({
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                return attr.id.length ? attr.id : __traits(identifier, T);
            }
        }
        return __traits(identifier, T);
    })();
    enum string name = ({
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                return attr.displayName.length ? attr.displayName : id;
            }
        }
        return id;
    })();
    enum int order = ({
        foreach (attr; __traits(getAttributes, T)) {
            static if (is(typeof(attr) == AMProcessor)) {
                return attr.order;
            }
        }
        return 0;
    })();
}
