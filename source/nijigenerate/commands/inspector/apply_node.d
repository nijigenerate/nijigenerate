module nijigenerate.commands.inspector.apply_node;

import nijigenerate.commands.base;
import nijigenerate;
import nijigenerate.ext; // ExCamera, ExPart, etc.
import nijigenerate.actions; // Action interface
import nijigenerate.core.actionstack; // incActionPush
import i18n;
import nijigenerate.panels.inspector.node;
import nijigenerate.panels.inspector.drawable;
import nijigenerate.panels.inspector.camera;
import nijigenerate.panels.inspector.composite;
import nijigenerate.panels.inspector.part;
import nijigenerate.panels.inspector.meshgroup;
import nijigenerate.panels.inspector.pathdeform;
import nijigenerate.panels.inspector.simplephysics;
// Inspector resolution must be via ctx.inspectors (no global resolver)
import nijilive; // Node, Drawable
import nijigenerate.commands.base : toCodeString;

// Generic apply command using NodeInspector; compile-time PropName
class ApplyInspectorPropCommand(I, string PropName) : ExCommand!(TW!(typeof(mixin("(cast(I)(null))."~PropName~".value")), "value", "Value to apply")) {
    // The constructor for ExCommand will be used.
    // It takes (string label, string desc, args...)
    // The `DefApply` template will generate a constructor that calls this.
    alias ValT  = typeof(mixin("(cast(I)(null))."~PropName~".value"));
    this(ValT value) {
        super("Apply " ~ PropName, value);
    }

    override void run(Context ctx) {
        I ni = null;
        if (ctx.hasInspectors) {
            foreach (i; ctx.inspectors) {
                ni = cast(I)i;
                if (ni !is null) break;
            }
        }
        if (ni is null) return;
        import std.traits : TemplateArgsOf;
        alias NodeT = TemplateArgsOf!I[1];

        NodeT[] nodes;
        bool isDeform = (ni.subMode() == ModelEditSubMode.Deform);
        
        ValT[] oldVals;
        if (!isDeform && ctx.hasNodes) {
            foreach (n; ctx.nodes) {
                if (auto t = cast(NodeT) n) nodes ~= t;
            }
            foreach (n; nodes) oldVals ~= mixin("ni."~PropName~".get(n)");
        }

        mixin("ni."~PropName~".value = this.value;");
        mixin("ni."~PropName~".apply();");

        // Set value and apply
        if (!isDeform && ctx.hasNodes) {
            bool changed = false;
            if (oldVals.length > 0 && oldVals[0] != this.value) {
                changed = true;
            }

            if (changed) {
                static class _AttrAction(NodeT2, ValT2) : Action {
                    I ni;
                    NodeT2[] nodes;
                    ValT2[] oldVals;
                    ValT2 newVal;
                    this(I ni, NodeT2[] nodes, ValT2[] oldVals, ValT2 newVal) {
                        this.ni = ni;
                        this.nodes = nodes;
                        this.oldVals = oldVals;
                        this.newVal = newVal;
                    }
                    override void rollback() {
                        foreach (i, n; nodes) {
                            mixin("ni."~PropName~".set(n, oldVals[i]);");
                            n.notifyChange(n, NotifyReason.AttributeChanged);
                        }
                    }
                    override void redo() {
                        foreach (i, n; nodes) {
                            mixin("ni."~PropName~".set(n, newVal);");
                            n.notifyChange(n, NotifyReason.AttributeChanged);
                        }
                    }
                    override string describe() { return "Changed " ~PropName; }
                    override string describeUndo() { return "Undo change " ~PropName; }
                    override string getName() { return this.stringof; }
                    
                    override bool canMerge(Action other) {
                        auto o = cast(_AttrAction!(NodeT2, ValT2)) other;
                        if (o is null) return false;
                        if (o.ni !is this.ni) return false;
                        if (o.nodes.length != this.nodes.length) return false;
                        foreach (i; 0 .. nodes.length) {
                            if (nodes[i].uuid != o.nodes[i].uuid) return false;
                        }
                        return true;
                    }
                    override bool merge(Action other) {
                        auto o = cast(_AttrAction!(NodeT2, ValT2)) other;
                        if (o is null) return false;
                        this.newVal = o.newVal;
                        return true;
                    }
                }
                incActionPush(new _AttrAction!(NodeT, ValT)(ni, nodes, oldVals, this.value));
            }
        }

        if (ctx.hasNodes)
            ni.capture(cast(Node[])ctx.nodes);
    }

    // Apply-style inspector commands require specifying values and are not suited for shortcuts
    override bool shortcutRunnable() { return false; }
}

class ToggleInspectorPropCommand(I, string PropName) : ExCommand!() {
    this() {
        super("Toggle " ~ PropName, "Toggles the value of " ~ PropName);
    }

    override void run(Context ctx) {
        I ni = null;
        if (ctx.hasInspectors) {
            foreach (i; ctx.inspectors) {
                ni = cast(I)i;
                if (ni !is null) break;
            }
        }
        if (ni is null) return;

        import std.traits : TemplateArgsOf;
        alias NodeT = TemplateArgsOf!I[1];
        alias ValT = typeof(mixin("(cast(I)(null))."~PropName~".value"));
        static assert(is(ValT == bool), "Toggle command only works for boolean properties");

        NodeT[] nodes;
        bool isDeform = (ni.subMode() == ModelEditSubMode.Deform);
        
        ValT[] oldVals;
        if (!isDeform && ctx.hasNodes) {
            foreach (n; ctx.nodes) {
                if (auto t = cast(NodeT) n) nodes ~= t;
            }
            foreach (n; nodes) oldVals ~= mixin("ni."~PropName~".get(n)");
        }

        mixin("ni." ~ PropName ~ ".value = !ni." ~ PropName ~ ".value;");
        mixin("ni." ~ PropName ~ ".apply();");

        // Set value and apply
        if (!isDeform && ctx.hasNodes && nodes.length > 0) {
            static class _ToggleAttrAction(NodeT2, ValT2) : Action {
                I ni;
                NodeT2[] nodes;
                ValT2[] oldVals;

                this(I ni, NodeT2[] nodes, ValT2[] oldVals) {
                    this.ni = ni;
                    this.nodes = nodes;
                    this.oldVals = oldVals;
                }

                override void rollback() {
                    foreach (i, n; nodes) {
                        mixin("ni."~PropName~".set(n, oldVals[i]);");
                        n.notifyChange(n, NotifyReason.AttributeChanged);
                    }
                }

                override void redo() {
                    // If all values are the same, toggle. Otherwise, set all to true. 
                    bool allSame = true;
                    if (oldVals.length > 1) {
                        foreach (i; 1 .. oldVals.length) {
                            if (oldVals[i] != oldVals[0]) {
                                allSame = false;
                                break;
                            }
                        }
                    }
                    
                    ValT2 newVal = true;
                    if (allSame) {
                        newVal = !oldVals[0];
                    }

                    foreach (i, n; nodes) {
                        mixin("ni."~PropName~".set(n, newVal);" );
                        n.notifyChange(n, NotifyReason.AttributeChanged);
                    }
                }

                override string describe() { return "Toggled " ~ PropName; }
                override string describeUndo() { return "Undo toggle " ~ PropName; }
                override string getName() { return this.stringof; }
                
                override bool canMerge(Action other) { return false; } // Toggle actions shouldn't merge
                override bool merge(Action other) { return false; }
            }
            incActionPush(new _ToggleAttrAction!(NodeT, ValT)(ni, nodes, oldVals));
        }

        if (ctx.hasNodes)
            ni.capture(cast(Node[])ctx.nodes);
    }
}

// Property-level commands enum (per-property IDs)
enum InspectorNodeApplyCommand {
    TranslationX,
    TranslationY,
    TranslationZ,
    RotationX,
    RotationY,
    RotationZ,
    ScaleX,
    ScaleY,
    PixelSnap,
    TogglePixelSnap,
    ZSort,
    ToggleZSort,
    LockToRoot,
    ToggleLockToRoot,
    PinToMesh,
    TogglePinToMesh,
    OffsetX,
    OffsetY,
    // Camera
    ViewportOrigin,
    // Composite
    CompositeTint,
    CompositeScreenTint,
    CompositeBlendingMode,
    CompositeOpacity,
    CompositeThreshold,
    // Part
    PartTint,
    PartScreenTint,
    PartEmissionStrength,
    PartBlendingMode,
    PartOpacity,
    PartMaskAlphaThreshold,
    PartAutoResizedMesh,
    TogglePartAutoResizedMesh,
    // MeshGroup
    MeshGroupDynamic,
    ToggleMeshGroupDynamic,
    MeshGroupTranslateChildren,
    ToggleMeshGroupTranslateChildren,
    // PathDeformer
    PathDeformDynamic,
    TogglePathDeformDynamic,
    PathDeformPhysicsEnabled,
    TogglePathDeformPhysicsEnabled,
    PathDeformGravity,
    PathDeformRestoreConstant,
    PathDeformDamping,
    PathDeformInputScale,
    PathDeformPropagateScale,
    // SimplePhysics
    SimplePhysicsModelType,
    SimplePhysicsMapMode,
    SimplePhysicsLocalOnly,
    ToggleSimplePhysicsLocalOnly,
    SimplePhysicsGravity,
    SimplePhysicsLength,
    SimplePhysicsFrequency,
    SimplePhysicsAngleDamping,
    SimplePhysicsLengthDamping,
    SimplePhysicsOutputScaleX,
    SimplePhysicsOutputScaleY,
}

Command[InspectorNodeApplyCommand] commands;

// Define concrete command types matching the enum naming convention (no static this)
alias NINode = nijigenerate.panels.inspector.node.NodeInspector!(ModelEditSubMode.Layout, Node);
alias NIDraw = nijigenerate.panels.inspector.drawable.NodeInspector!(ModelEditSubMode.Layout, Drawable);
alias NICam  = nijigenerate.panels.inspector.camera.NodeInspector!(ModelEditSubMode.Layout, ExCamera);
alias NICmp  = nijigenerate.panels.inspector.composite.NodeInspector!(ModelEditSubMode.Layout, Composite);
alias NIPart = nijigenerate.panels.inspector.part.NodeInspector!(ModelEditSubMode.Layout, Part);
alias NIMesh = nijigenerate.panels.inspector.meshgroup.NodeInspector!(ModelEditSubMode.Layout, MeshGroup);
alias NIPath = nijigenerate.panels.inspector.pathdeform.NodeInspector!(ModelEditSubMode.Layout, PathDeformer);
alias NISPhys = nijigenerate.panels.inspector.simplephysics.NodeInspector!(ModelEditSubMode.Layout, SimplePhysics);

template DefApply(string Id, alias I, string Prop) {
    enum string s_base = "class " ~ Id ~ "Command : ApplyInspectorPropCommand!(" ~ I.stringof ~ ", \"" ~ Prop ~ "\") {" ~
                   "    this() { super(ApplyInspectorPropCommand!(" ~ I.stringof ~ ", \"" ~ Prop ~ "\").ValT.init); }" ~
                   "}";

    alias ValT = typeof(mixin("(cast(" ~ I.stringof ~ ")(null))."~Prop~".value"));
    static if (is(ValT == bool)) {
        enum string s_toggle = "class Toggle" ~ Id ~ "Command : ToggleInspectorPropCommand!(" ~ I.stringof ~ ", \"" ~ Prop ~ "\") {}";
        enum DefApply = s_base ~ s_toggle;
    } else {
        enum DefApply = s_base;
    }
}
        

mixin(DefApply!("TranslationX", NINode, "translationX"));
mixin(DefApply!("TranslationY", NINode, "translationY"));
mixin(DefApply!("TranslationZ", NINode, "translationZ"));
mixin(DefApply!("RotationX",    NINode, "rotationX"));
mixin(DefApply!("RotationY",    NINode, "rotationY"));
mixin(DefApply!("RotationZ",    NINode, "rotationZ"));
mixin(DefApply!("ScaleX",       NINode, "scaleX"));
mixin(DefApply!("ScaleY",       NINode, "scaleY"));
mixin(DefApply!("PixelSnap",    NINode, "pixelSnap"));
mixin(DefApply!("ZSort",        NINode, "zSort"));
mixin(DefApply!("PinToMesh",    NINode, "pinToMesh"));
mixin(DefApply!("OffsetX",      NIDraw, "offsetX"));
mixin(DefApply!("OffsetY",      NIDraw, "offsetY"));
mixin(DefApply!("LockToRoot",   NINode, "lockToRoot"));

// Camera
mixin(DefApply!("ViewportOrigin", NICam,  "viewportOrigin"));

// Composite
mixin(DefApply!("CompositeTint",         NICmp,  "tint"));
mixin(DefApply!("CompositeScreenTint",   NICmp,  "screenTint"));
mixin(DefApply!("CompositeBlendingMode", NICmp,  "blendingMode"));
mixin(DefApply!("CompositeOpacity",      NICmp,  "opacity"));
mixin(DefApply!("CompositeThreshold",    NICmp,  "threshold"));

// Part
mixin(DefApply!("PartTint",              NIPart, "tint"));
mixin(DefApply!("PartScreenTint",        NIPart, "screenTint"));
mixin(DefApply!("PartEmissionStrength",  NIPart, "emissionStrength"));
mixin(DefApply!("PartBlendingMode",      NIPart, "blendingMode"));
mixin(DefApply!("PartOpacity",           NIPart, "opacity"));
mixin(DefApply!("PartMaskAlphaThreshold",NIPart, "maskAlphaThreshold"));
mixin(DefApply!("PartAutoResizedMesh",   NIPart, "autoResizedMesh"));

// MeshGroup
mixin(DefApply!("MeshGroupDynamic",           NIMesh, "dynamic"));
mixin(DefApply!("MeshGroupTranslateChildren", NIMesh, "translateChildren"));

// PathDeformer
mixin(DefApply!("PathDeformDynamic",         NIPath, "dynamic"));
mixin(DefApply!("PathDeformPhysicsEnabled",  NIPath, "physicsEnabled"));
mixin(DefApply!("PathDeformGravity",         NIPath, "gravity"));
mixin(DefApply!("PathDeformRestoreConstant", NIPath, "restoreConstant"));
mixin(DefApply!("PathDeformDamping",         NIPath, "damping"));
mixin(DefApply!("PathDeformInputScale",      NIPath, "inputScale"));
mixin(DefApply!("PathDeformPropagateScale",  NIPath, "propagateScale"));

// SimplePhysics
mixin(DefApply!("SimplePhysicsModelType",     NISPhys, "modelType"));
mixin(DefApply!("SimplePhysicsMapMode",       NISPhys, "mapMode"));
mixin(DefApply!("SimplePhysicsLocalOnly",     NISPhys, "localOnly"));
mixin(DefApply!("SimplePhysicsGravity",       NISPhys, "gravity"));
mixin(DefApply!("SimplePhysicsLength",        NISPhys, "length"));
mixin(DefApply!("SimplePhysicsFrequency",     NISPhys, "frequency"));
mixin(DefApply!("SimplePhysicsAngleDamping",  NISPhys, "angleDamping"));
mixin(DefApply!("SimplePhysicsLengthDamping", NISPhys, "lengthDamping"));
mixin(DefApply!("SimplePhysicsOutputScaleX",  NISPhys, "outputScaleX"));
mixin(DefApply!("SimplePhysicsOutputScaleY",  NISPhys, "outputScaleY"));

void ngInitCommands(T)() if (is(T == InspectorNodeApplyCommand)) {
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!InspectorNodeApplyCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
