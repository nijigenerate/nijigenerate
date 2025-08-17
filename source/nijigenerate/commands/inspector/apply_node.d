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

// Generic apply command using NodeInspector; compile-time PropName
class ApplyInspectorPropCommand(I, string PropName) : ExCommand!() {
    this() { super("Apply " ~ PropName); }
    override void run(Context ctx) {
        I ni = null;
        // Prefer inspectors provided via ctx
        import std.stdio : writefln;
        writefln("ctx.inspectors=%s", ctx.inspectors);
        if (ctx.hasInspectors) {
            foreach (i; ctx.inspectors) {
                ni = cast(I)i;
                if (ni !is null) break;
            }
        }
        // No fallback to global resolver: require ctx.inspectors
        // debug print for inspector resolution (keep for debugging)
        writefln("ni=%s(%x)", ni, &ni);
        if (ni is null) return;
        import std.traits : TemplateArgsOf;
        // infer node type parameter from inspector I
        alias NodeT = TemplateArgsOf!I[1];
        alias ValT  = typeof(mixin("ni."~PropName~".value"));

        NodeT[] nodes;
        // Decide by subMode: Deform => no extra action, Layout => attribute action
        bool isDeform = false;
        static if (__traits(compiles, { ni.subMode(); })) {
            isDeform = (ni.subMode() == ModelEditSubMode.Deform);
        }
        ValT[] oldVals;
        ValT[] newVals;
        if (!isDeform && ctx.hasNodes) {
            foreach (n; ctx.nodes) {
                if (auto t = cast(NodeT) n) nodes ~= t;
            }
            foreach (n; nodes) oldVals ~= mixin("ni."~PropName~".get(n)");
        }

        mixin("ni." ~ PropName ~ ".apply();");

        if (!isDeform && ctx.hasNodes) {
            // capture new values after apply
            foreach (n; nodes) {
                newVals ~= mixin("ni."~PropName~".get(n)");
            }

            // push undo/redo action if anything changed
            bool changed = false;
            if (oldVals.length == newVals.length) {
                foreach (i; 0 .. oldVals.length) {
                    if (oldVals[i] != newVals[i]) { changed = true; break; }
                }
            }
            if (changed) {
                static class _AttrAction(NodeT2, ValT2) : Action {
                    I ni;
                    NodeT2[] nodes;
                    ValT2[] oldVals;
                    ValT2[] newVals;
                    this(I ni, NodeT2[] nodes, ValT2[] oldVals, ValT2[] newVals) {
                        this.ni = ni;
                        this.nodes = nodes;
                        this.oldVals = oldVals;
                        this.newVals = newVals;
                    }
                    override void rollback() {
                        foreach (i, n; nodes) {
                            mixin("ni."~PropName~".set(n, oldVals[i]);");
                            n.notifyChange(n, NotifyReason.AttributeChanged);
                        }
                    }
                    override void redo() {
                        foreach (i, n; nodes) {
                            mixin("ni."~PropName~".set(n, newVals[i]);");
                            n.notifyChange(n, NotifyReason.AttributeChanged);
                        }
                    }
                    override string describe() { return "Changed "~PropName; }
                    override string describeUndo() { return "Undo change "~PropName; }
                    override string getName() { return this.stringof; }
                    override bool canMerge(Action other) {
                        auto o = cast(_AttrAction!(NodeT2, ValT2)) other;
                        if (o is null) return false;
                        // Same inspector instance and same target nodes (by uuid and order)
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
                        // Keep original oldVals, update newVals to the newer action's values
                        this.newVals = o.newVals.dup;
                        return true;
                    }
                }
                incActionPush(new _AttrAction!(NodeT, ValT)(ni, nodes, oldVals, newVals));
            }
        }

        // Keep inspector values in sync across selections when nodes are provided
        static if (__traits(compiles, { ctx.nodes; })) {
            if (ctx.hasNodes)
                ni.capture(cast(Node[])ctx.nodes);
        }
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
    ZSort,
    LockToRoot,
    PinToMesh,
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
    // MeshGroup
    MeshGroupDynamic,
    MeshGroupTranslateChildren,
    // PathDeformer
    PathDeformDynamic,
    PathDeformPhysicsEnabled,
    PathDeformGravity,
    PathDeformRestoreConstant,
    PathDeformDamping,
    PathDeformInputScale,
    PathDeformPropagateScale,
    // SimplePhysics
    SimplePhysicsModelType,
    SimplePhysicsMapMode,
    SimplePhysicsLocalOnly,
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
    enum DefApply =
        "class "~Id~"Command : ApplyInspectorPropCommand!("~I.stringof~", \""~Prop~"\") {\n"~
        "}\n";
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

void ngInitCommands(T)() if (is(T == InspectorNodeApplyCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!InspectorNodeApplyCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
