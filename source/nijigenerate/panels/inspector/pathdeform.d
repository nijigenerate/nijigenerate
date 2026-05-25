module nijigenerate.panels.inspector.pathdeform;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.depth.bone : DepthBoneDirtyScope, ngMarkDepthBoneDirty, ngMarkDepthBoneDirtyForArmedParameter;
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.project : incActivePuppet, incArmedParameter, incSelectedNodes;
import nijilive;
import std.format;
import std.utf;
import std.string;
import std.conv : to;
import i18n;


/// Model View

class NodeInspector(ModelEditSubMode inspectorMode, T: PathDeformer) : BaseInspector!(inspectorMode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    static if (inspectorMode == ModelEditSubMode.Layout) {
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

                if (ngCheckbox(__("Dynamic mode(slow)"), &dynamic.value)) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.PathDeformDynamic)(ctx, dynamic.value);
                }
                if (ngCheckbox(__("Auto-Physics"), &physicsEnabled.value)) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.PathDeformPhysicsEnabled)(ctx, physicsEnabled.value);
                }
                incTooltip(_("Enabled / Disabled physics driver for vertices. If enabled, vertices are moved along with specified physics engine."));

            string curveTypeText = curveType.value == CurveType.Bezier ? "Bezier" : curveType.value == CurveType.Spline? "Spline" : "Invalid";
            if (igBeginCombo("###PhysType", __(curveTypeText))) {
                bool curveTypeChanged = false;

                if (igSelectable(__("Bezier"), curveType.value == CurveType.Bezier)) {
                    curveType.value = CurveType.Bezier;
                    curveTypeChanged = true;
                }

                if (igSelectable(__("Spline"), curveType.value == CurveType.Spline)) {
                    curveType.value = CurveType.Spline;
                    curveTypeChanged = true;
                }

                if (curveTypeChanged) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.PathDeformCurveType)(ctx, curveType.value);
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
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.PathDeformGravity)(ctx, gravity.value);
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(1);
                        incText(_("Restore force"));
                        incTooltip(_("Force to restore for original position. If this force is weaker than the gravity, pendulum cannot restore to original position."));
                        if (_shared!restoreConstant(()=>incDragFloat("restoreConstant", &restoreConstant.value, adjustSpeed/100, 0.01, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.PathDeformRestoreConstant)(ctx, restoreConstant.value);
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(2);
                        incText(_("Damping"));
                        if (_shared!damping(()=>incDragFloat("damping", &damping.value, adjustSpeed/100, 0, 5, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.PathDeformDamping)(ctx, damping.value);
                        }
                    igPopID();

                    igPushID(3);
                        incText(_("Input scale"));
                        incTooltip(_("Input force is multiplied by this factor. This should be specified when original position moved too much."));
                        if (_shared!inputScale(()=>incDragFloat("inputScale", &inputScale.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.PathDeformInputScale)(ctx, inputScale.value);
                        }
                        igSpacing();
                        igSpacing();
                    igPopID();

                    igPushID(4);
                        incText(_("Propagate scale"));
                        incTooltip(_("Specify the degree to convey movement of previous pendulum to next one."));
                        if (_shared!propagateScale(()=>incDragFloat("propagateScale", &propagateScale.value, adjustSpeed/100, 0, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.PathDeformPropagateScale)(ctx, propagateScale.value);
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

            drawDepthBoneSources(node);
        }
    } else static if (inspectorMode == ModelEditSubMode.Deform) {
        override
        void run(Parameter parameter, vec2u cursor) {
            if (targets.length == 0) return;
            drawDepthBoneSources(targets[0], parameter, cursor);
        }
    }

    mixin MultiEdit;
    mixin(attribute!(bool, "dynamic",   (x)=>x~".dynamic", (x, v)=>x~".switchDynamic("~v~")"));
    mixin(attribute!(bool, "physicsEnabled",   (x   )=>x~".physicsEnabled", (x, v)=>x~".physicsEnabled = "~v));
    mixin(attribute!(CurveType, "curveType",   (x)=>x~".curveType", (x, v)=>x~".curveType = "~v~"; "~x~".rebuffer("~x~".vertices)"));
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

    void drawDepthBoneSources(PathDeformer node, Parameter parameter = null, vec2u cursor = vec2u.init) {
        auto root = findDepthRigRoot();
        if (incBeginCategory(__("Depth Bone Sources"))) {
            if (root is null) {
                igText(__("No DepthRigRoot."));
            } else {
                if (incButtonColored("")) {
                    auto ctx = new Context(); ctx.nodes([cast(Node)root]);
                    cmd!(DepthBoneCommand.AddStandardDepthSkeleton)(ctx, root, 1.0f);
                }
                incTooltip(_("Add Standard Skeleton"));

                auto index = root.findBindingIndex(node.uuid);
                incText(_("Depth Bone Sources"));
                if (igBeginListBox("###DepthBoneSources", ImVec2(0, 128))) {
                    if (index < 0 || root.bindings[cast(size_t)index].sourceBoneUuids.length == 0) {
                        incText(_("(Drag a Depth Bone Here)"));
                    } else {
                        ulong removeUuid = 0;
                        ExDepthBone previewBone;
                        auto binding = &root.bindings[cast(size_t)index];
                        binding.normalizeSourceSettings();
                        foreach (i, uuid; binding.sourceBoneUuids) {
                            auto bone = findDepthBone(root, uuid);
                            auto setting = binding.sourceSetting(uuid);
                            igPushID(cast(int)i);
                                if (igBeginPopup("###DepthBoneSourceSettings")) {
                                    if (bone !is null && igMenuItem(__("Focus"))) {
                                        incFocusCamera(bone);
                                        incSelectNode(bone);
                                    }
                                    if (bone !is null && igMenuItem(__("Preview"))) {
                                        previewBone = bone;
                                    }
                                    igSeparator();
                                    float weight = setting.weight;
                                    if (igDragFloat(__("Weight"), &weight, 0.01f, 0.0f, 1.0f, "%.3f")) {
                                        setting.weight = weight;
                                        if (bone !is null) setDepthBoneSourceSettings(root, node, bone, setting, parameter, cursor);
                                    }
                                    float depthOffset = setting.depthOffset;
                                    if (igDragFloat(__("Depth Offset"), &depthOffset, 0.01f, -10.0f, 10.0f, "%.3f")) {
                                        setting.depthOffset = depthOffset;
                                        if (bone !is null) setDepthBoneSourceSettings(root, node, bone, setting, parameter, cursor);
                                    }
                                    float depthScale = setting.depthScale;
                                    if (igDragFloat(__("Depth Scale"), &depthScale, 0.01f, 0.01f, 10.0f, "%.3f")) {
                                        setting.depthScale = depthScale;
                                        if (bone !is null) setDepthBoneSourceSettings(root, node, bone, setting, parameter, cursor);
                                    }
                                    igSeparator();
                                    if (igMenuItem(__("Delete"))) {
                                        removeUuid = uuid;
                                        igEndPopup();
                                        igPopID();
                                        break;
                                    }
                                    igEndPopup();
                                }

                                igSelectable((bone !is null ? bone.name : ("#" ~ uuid.to!string)).toStringz, false, ImGuiSelectableFlags.AllowItemOverlap, ImVec2(0, 17));

                                if (igBeginDragDropTarget()) {
                                    const(ImGuiPayload)* payload = igAcceptDragDropPayload("_DEPTHBONEITEM");
                                    if (payload !is null) {
                                        auto draggedUuid = *cast(ulong*)payload.Data;
                                        reorderDepthBoneSource(root, node, draggedUuid, uuid, parameter, cursor);
                                    }
                                    igEndDragDropTarget();
                                }

                                if (igIsItemClicked(ImGuiMouseButton.Right)) {
                                    igOpenPopup("###DepthBoneSourceSettings");
                                }

                                igSameLine(0, 0);
                                if (igBeginChild("###DepthBoneSourceDepth", ImVec2(0, 17), false, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.AlwaysAutoResize)) {
                                    incDummy(ImVec2(-144, 1));
                                    igSameLine(0, 0);
                                    auto depthOffset = setting.depthOffset;
                                    igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(0, 1));
                                    igSetNextItemWidth(72);
                                    if (igDragFloat("###offset", &depthOffset, 0.01f, -10.0f, 10.0f, "o %.2f")) {
                                        setting.depthOffset = depthOffset;
                                        if (bone !is null) setDepthBoneSourceSettings(root, node, bone, setting, parameter, cursor);
                                    }
                                    igSameLine(0, 0);
                                    auto depthScale = setting.depthScale;
                                    igSetNextItemWidth(72);
                                    if (igDragFloat("###scale", &depthScale, 0.01f, 0.01f, 10.0f, "s %.2f")) {
                                        setting.depthScale = depthScale;
                                        if (bone !is null) setDepthBoneSourceSettings(root, node, bone, setting, parameter, cursor);
                                    }
                                    igPopStyleVar();
                                }
                                igEndChild();

                                if (igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                                    auto payloadUuid = uuid;
                                    igSetDragDropPayload("_DEPTHBONEITEM", cast(void*)&payloadUuid, ulong.sizeof, ImGuiCond.Always);
                                    incText(bone !is null ? bone.name : ("#" ~ uuid.to!string));
                                    igEndDragDropSource();
                                }
                            igPopID();
                        }

                        if (previewBone !is null) {
                            auto ctx = new Context(); ctx.nodes([cast(Node)node]);
                            cmd!(DepthBoneCommand.PreviewDepthBoneInfluence)(ctx, root, node, previewBone);
                        }
                        if (removeUuid != 0) {
                            if (auto bone = findDepthBone(root, removeUuid)) {
                                auto ctx = new Context(); ctx.nodes([cast(Node)node]);
                                cmd!(DepthBoneCommand.RemoveDepthBoneSource)(ctx, root, node, bone);
                            }
                        }
                    }

                    igEndListBox();
                }

                if (igBeginDragDropTarget()) {
                    const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                    if (payload !is null) {
                        if (auto bone = cast(ExDepthBone)*cast(Node*)payload.Data) {
                            auto ctx = new Context(); ctx.nodes([cast(Node)node]);
                            cmd!(DepthBoneCommand.AddDepthBoneSource)(ctx, root, node, bone);
                        }
                    }
                    igEndDragDropTarget();
                }

                auto armedParam = incArmedParameter();
                if (armedParam !is null) {
                    if (incButtonColored("")) {
                        auto ctx = new Context(); ctx.nodes([cast(Node)node]); ctx.armedParameters = [armedParam];
                        cmd!(DepthBoneCommand.PreviewDepthBoneDeform)(ctx, root, [cast(Node)node]);
                    }
                    incTooltip(_("Preview Depth Bone Deform"));
                    igSameLine();
                    if (incButtonColored("")) {
                        auto ctx = new Context(); ctx.nodes([cast(Node)node]); ctx.armedParameters = [armedParam];
                        cmd!(DepthBoneCommand.ApplyDepthBoneDeform)(ctx, root, [cast(Node)node]);
                    }
                    incTooltip(_("Apply Depth Bone Deform"));
                }

                auto binding = root.getOrCreateBinding(node, ExDepthTargetKind.Path);
                int maxInfluences = cast(int)binding.influenceRule.maxInfluences;
                float radiusScale = binding.influenceRule.radiusScale;
                float minimumRadius = binding.influenceRule.minimumRadius;
                bool ruleChanged = false;
                igSpacing();
                igTextColored(CategoryTextColor, __("Influence Rule"));
                ruleChanged = igDragInt("Max Influences", &maxInfluences, 0.1f, 1, 16) || ruleChanged;
                ruleChanged = igDragFloat("Radius Scale", &radiusScale, 0.01f, 0.01f, 100.0f, "%.3f") || ruleChanged;
                ruleChanged = igDragFloat("Minimum Radius", &minimumRadius, 0.1f, 0.0f, 100000.0f, "%.2f") || ruleChanged;
                if (ruleChanged) {
                    if (maxInfluences < 1) maxInfluences = 1;
                    auto ctx = new Context(); ctx.nodes([cast(Node)node]);
                    cmd!(DepthBoneCommand.SetDepthBoneInfluenceRule)(
                        ctx,
                        root,
                        node,
                        format(`{"maxInfluences":%s,"radiusScale":%s,"minimumRadius":%s}`, maxInfluences, radiusScale, minimumRadius)
                    );
                }
            }
        }
        incEndCategory();
    }

    static ExDepthRigRoot findDepthRigRoot() {
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        ExDepthRigRoot found;
        void visit(Node n) {
            if (found !is null || n is null) return;
            if (auto root = cast(ExDepthRigRoot)n) {
                found = root;
                return;
            }
            foreach (child; n.children) visit(child);
        }
        visit(puppet.root);
        return found;
    }

    static ExDepthBone findDepthBone(ExDepthRigRoot root, ulong uuid) {
        foreach (bone; root.depthBones()) if (bone.uuid == uuid) return bone;
        return null;
    }

    static void reorderDepthBoneSource(ExDepthRigRoot root, Node target, ulong fromUuid, ulong toUuid, Parameter parameter = null, vec2u cursor = vec2u.init) {
        if (root is null || target is null || fromUuid == toUuid) return;
        auto index = root.findBindingIndex(target.uuid);
        if (index < 0) return;
        auto oldBindings = root.bindings.dup;
        auto binding = &root.bindings[cast(size_t)index];
        bool hasFrom = false;
        bool hasTo = false;
        foreach (uuid; binding.sourceBoneUuids) {
            if (uuid == fromUuid) hasFrom = true;
            if (uuid == toUuid) hasTo = true;
        }
        if (!hasFrom || !hasTo) return;

        ulong[] reordered;
        foreach (uuid; binding.sourceBoneUuids) {
            if (uuid == fromUuid) continue;
            if (uuid == toUuid) reordered ~= fromUuid;
            reordered ~= uuid;
        }
        binding.sourceBoneUuids = reordered;
        binding.normalizeSourceSettings();
        incActionPush(new DepthBoneSourceListChangeAction("Reorder Depth Bone Source", root, oldBindings, root.bindings));
        if (parameter !is null) {
            ngMarkDepthBoneDirty(root, parameter, cursor, "Reorder Depth Bone Source", DepthBoneDirtyScope.AllKeypoints);
        } else {
            ngMarkDepthBoneDirtyForArmedParameter(root, "Reorder Depth Bone Source", DepthBoneDirtyScope.AllKeypoints);
        }
    }

    static void setDepthBoneSourceSettings(ExDepthRigRoot root, Node target, ExDepthBone bone, ExDepthBoneSourceSettings setting, Parameter parameter = null, vec2u cursor = vec2u.init) {
        auto ctx = new Context(); ctx.nodes([target]);
        if (parameter !is null) ctx.armedParameters = [parameter];
        cmd!(DepthBoneCommand.SetDepthBoneSourceSettings)(
            ctx,
            root,
            target,
            bone,
            format(`{"weight":%s,"depthOffset":%s,"depthScale":%s}`, setting.weight, setting.depthOffset, setting.depthScale)
        );
    }
}
