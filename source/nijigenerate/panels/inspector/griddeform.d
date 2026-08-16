module nijigenerate.panels.inspector.griddeform;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.actions;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.depth.bone : ngBeginDepthBoneSourceSettingsMerge,
    ngCreateDepthBoneSourceSettingsMergeSession, ngEndDepthBoneSourceSettingsMerge;
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.project : incActivePuppet, incArmedParameter, incSelectedNodes;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import i18n;
import std.algorithm : sort, map;
import std.algorithm.iteration : uniq;
import std.array : array;
import std.conv : to;
import std.format : format;
import std.string;

/// Model View

class NodeInspector(ModelEditSubMode inspectorMode, T: GridDeformer) : BaseInspector!(inspectorMode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    static if (inspectorMode == ModelEditSubMode.Layout) {
        override
        void run() {
            if (targets.length == 0) return;
            auto node = targets[0];
            if (incBeginCategory(__("GridDeformer"))) {

                igSpacing();

                if (_shared!dynamic(()=>ngCheckbox(__("Dynamic Deformation (slower)"), &dynamic.value))) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.GridDeformDynamic)(ctx, dynamic.value);
                }
                incTooltip(_("Whether the GridDeformer should dynamically deform its targets. Enabling this has a performance cost."));

                igSpacing();

                auto baseVerts = node.vertices;
                igTextColored(CategoryTextColor, __("Grid Information"));
                igText(__("Vertices: %d"), cast(int)baseVerts.length);

                size_t cols = 0;
                size_t rows = 0;
                bool validGrid = false;
                if (baseVerts.length >= 4) {
                    auto baseArray = baseVerts.toArray();
                    auto xs = baseArray.map!(v => v.x).array;
                    auto ys = baseArray.map!(v => v.y).array;
                    xs.sort();
                    ys.sort();
                    xs = xs.uniq.array;
                    ys = ys.uniq.array;
                    cols = xs.length;
                    rows = ys.length;
                    validGrid = cols >= 2 && rows >= 2 && cols * rows == baseVerts.length;
                }

                if (validGrid) {
                    igText(__("Columns: %d"), cast(int)cols);
                    igText(__("Rows: %d"), cast(int)rows);
                } else {
                    igText(__("Grid axes are not initialized."));
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
    mixin(attribute!(bool, "dynamic", (x)=>x~".dynamic", (x, v)=>x~".switchDynamic("~v~")"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        dynamic.capture();
    }

private:
    void drawDepthBoneSources(GridDeformer node, Parameter parameter = null, vec2u cursor = vec2u.init) {
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
                                    auto weightChanged = igDragFloat(__("Weight"), &weight, 0.01f, 0.0f, 1.0f, "%.3f");
                                    auto weightSession = depthBoneSourceSettingsMergeSession(root, node, bone, "weight");
                                    if (weightChanged) {
                                        setting.weight = weight;
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "weight", weightSession, parameter, cursor);
                                    }
                                    float depthOffset = setting.depthOffset;
                                    auto depthOffsetChanged = igDragFloat(__("Depth Offset"), &depthOffset, 0.01f, -10.0f, 10.0f, "%.3f");
                                    auto depthOffsetSession = depthBoneSourceSettingsMergeSession(root, node, bone, "depthOffset");
                                    if (depthOffsetChanged) {
                                        setting.depthOffset = depthOffset;
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "depthOffset", depthOffsetSession, parameter, cursor);
                                    }
                                    float depthScale = setting.depthScale;
                                    auto depthScaleChanged = igDragFloat(__("Depth Scale"), &depthScale, 0.01f, 0.01f, 10.0f, "%.3f");
                                    auto depthScaleSession = depthBoneSourceSettingsMergeSession(root, node, bone, "depthScale");
                                    if (depthScaleChanged) {
                                        setting.depthScale = depthScale;
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "depthScale", depthScaleSession, parameter, cursor);
                                    }
                                    float rotationDegrees = degrees(setting.rotation);
                                    auto rotationChanged = igDragFloat(__("Rotation"), &rotationDegrees, 0.01f,
                                        -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat);
                                    auto rotationSession = depthBoneSourceSettingsMergeSession(root, node, bone, "rotation");
                                    if (rotationChanged) {
                                        setting.rotation = normalizeDepthBoneSourceRotation(radians(rotationDegrees));
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "rotation", rotationSession, parameter, cursor);
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
                                    incDummy(ImVec2(-168, 1));
                                    igSameLine(0, 0);
                                    auto depthOffset = setting.depthOffset;
                                    igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(0, 1));
                                    igSetNextItemWidth(56);
                                    auto depthOffsetChanged = igDragFloat("###offset", &depthOffset, 0.01f, -10.0f, 10.0f, "o %.2f");
                                    auto depthOffsetSession = depthBoneSourceSettingsMergeSession(root, node, bone, "depthOffset");
                                    if (depthOffsetChanged) {
                                        setting.depthOffset = depthOffset;
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "depthOffset", depthOffsetSession, parameter, cursor);
                                    }
                                    igSameLine(0, 0);
                                    auto depthScale = setting.depthScale;
                                    igSetNextItemWidth(56);
                                    auto depthScaleChanged = igDragFloat("###scale", &depthScale, 0.01f, 0.01f, 10.0f, "s %.2f");
                                    auto depthScaleSession = depthBoneSourceSettingsMergeSession(root, node, bone, "depthScale");
                                    if (depthScaleChanged) {
                                        setting.depthScale = depthScale;
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "depthScale", depthScaleSession, parameter, cursor);
                                    }
                                    igSameLine(0, 0);
                                    float rotationDegrees = degrees(setting.rotation);
                                    igSetNextItemWidth(56);
                                    auto rotationChanged = igDragFloat("###rotation", &rotationDegrees, 0.01f,
                                        -float.max, float.max, "r %.1f°", ImGuiSliderFlags.NoRoundToFormat);
                                    auto rotationSession = depthBoneSourceSettingsMergeSession(root, node, bone, "rotation");
                                    if (rotationChanged) {
                                        setting.rotation = normalizeDepthBoneSourceRotation(radians(rotationDegrees));
                                        if (bone !is null) setDepthBoneSourceSettingsInteractive(root, node, bone, setting, "rotation", rotationSession, parameter, cursor);
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
                    if (incButtonColored("\ue693")) {
                        auto ctx = new Context(); ctx.nodes([cast(Node)node]); ctx.armedParameters = [armedParam];
                        cmd!(DepthBoneCommand.PreviewDepthBoneDeform)(ctx, root, [cast(Node)node]);
                    }
                    incTooltip(_("Preview Depth Bone Deform"));
                    igSameLine();
                    if (incButtonColored("\ue668")) {
                        auto ctx = new Context(); ctx.nodes([cast(Node)node]); ctx.armedParameters = [armedParam];
                        cmd!(DepthBoneCommand.ApplyDepthBoneDeform)(ctx, root, [cast(Node)node]);
                    }
                    incTooltip(_("Apply Depth Bone Deform"));
                }

                auto binding = root.getOrCreateBinding(node, ExDepthTargetKind.Grid);
                int maxInfluences = cast(int)binding.influenceRule.maxInfluences;
                float radiusScale = binding.influenceRule.radiusScale;
                float minimumRadius = binding.influenceRule.minimumRadius;
                bool ruleChanged = false;
                igSpacing();
                igTextColored(CategoryTextColor, __("Influence Rule"));
                igPushItemWidth(120);
                ruleChanged = igDragInt(__("Max Influencing Bones"), &maxInfluences, 0.1f, 1, 16) || ruleChanged;
                ruleChanged = igDragFloat(__("Influence Radius Scale"), &radiusScale, 0.01f, 0.01f, 100.0f, "%.3f") || ruleChanged;
                ruleChanged = igDragFloat(__("Minimum Influence Radius"), &minimumRadius, 0.1f, 0.0f, 100000.0f, "%.2f") || ruleChanged;
                igPopItemWidth();
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
        incActionPush(new DepthBoneSourceListChangeAction(
            "Reorder Depth Bone Source", root, oldBindings, root.bindings, false, target));
    }

    static void setDepthBoneSourceSettings(ExDepthRigRoot root, Node target, ExDepthBone bone, ExDepthBoneSourceSettings setting, Parameter parameter = null, vec2u cursor = vec2u.init) {
        auto ctx = new Context(); ctx.nodes([target]);
        if (parameter !is null) ctx.armedParameters = [parameter];
        cmd!(DepthBoneCommand.SetDepthBoneSourceSettings)(
            ctx,
            root,
            target,
            bone,
            format(
                `{"weight":%s,"depthOffset":%s,"depthScale":%s,"rotation":%s}`,
                setting.weight,
                setting.depthOffset,
                setting.depthScale,
                setting.rotation,
            )
        );
    }

    static ulong[string] depthBoneSourceSettingsMergeSessions;

    static ulong depthBoneSourceSettingsMergeSession(
        ExDepthRigRoot root,
        Node target,
        ExDepthBone bone,
        string property,
    ) {
        if (root is null || target is null || bone is null) return 0;
        auto key = "%s:%s:%s:%s".format(root.uuid, target.uuid, bone.uuid, property);
        if (igIsItemActivated())
            depthBoneSourceSettingsMergeSessions[key] = ngCreateDepthBoneSourceSettingsMergeSession();
        auto found = key in depthBoneSourceSettingsMergeSessions;
        auto session = found is null ? 0 : *found;
        if (igIsItemDeactivatedAfterEdit())
            depthBoneSourceSettingsMergeSessions.remove(key);
        return session;
    }

    static void setDepthBoneSourceSettingsInteractive(
        ExDepthRigRoot root,
        Node target,
        ExDepthBone bone,
        ExDepthBoneSourceSettings setting,
        string property,
        ulong session,
        Parameter parameter = null,
        vec2u cursor = vec2u.init,
    ) {
        ngBeginDepthBoneSourceSettingsMerge(session, property);
        scope(exit) ngEndDepthBoneSourceSettingsMerge();
        setDepthBoneSourceSettings(root, target, bone, setting, parameter, cursor);
    }
}
