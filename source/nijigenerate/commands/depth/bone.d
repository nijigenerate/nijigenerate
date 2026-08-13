module nijigenerate.commands.depth.bone;

import nijigenerate.commands.base;
import nijigenerate.commands.depth.bone_gpu_async : DepthBoneGpuDispatchPacket, NgDepthBoneGpuAsyncResult,
    ngDepthBoneGpuAsyncMissingRequirements, ngDepthBoneGpuAsyncSupported, ngPendingDepthBoneGpuAsyncJobCount,
    ngPollDepthBoneGpuAsync, ngSubmitDepthBoneGpuAsync;
import nijigenerate.actions;
import nijigenerate.actions.depthboneinvalidation :
    DepthBoneMutation,
    DepthBoneMutationKind,
    ngDepthBoneMutationHook;
import nijigenerate.actions.parameter : ParameterChangeBindingsValueAction, ParameterShapeChangeAction;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.ext : ExParameter;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.project : incActivePuppet, incArmedParameter;
import nijigenerate.core.tasks : incSetStatus;
import nijigenerate.viewport.depth.common.targetview : ngDepthDisplayScaleForTargetsInNodeSpace;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import nijilive.core.param.binding : DeformationParameterBinding, ParameterBinding, ValueParameterBinding;
import nijilive.math;
import i18n;

import std.algorithm.comparison : max, min;
import std.algorithm.searching : countUntil;
import std.algorithm.sorting : sort;
import std.array : join;
import std.exception : enforce;
import std.json : JSONType, JSONValue, parseJSON;
import std.math : abs, cos, isFinite, sin, sqrt;
import std.conv : to;
import std.string : format, startsWith;

private enum EnableDepthBoneDebugLog = false;
enum DepthBoneGpuBoneStride = 24u;
enum DepthBoneGpuSourceStride = 28u;
enum DepthBoneGpuSourceDepthOffsetIndex = 6u;
enum DepthBoneGpuSourceSinRotationIndex = 8u;
enum DepthBoneGpuSourceCosRotationIndex = 9u;
enum DepthBoneGpuSourceRotationPivotXShiftIndex = 10u;
enum DepthBoneGpuSourcePoseYawIndex = 11u;
enum DepthBoneGpuSourceNoYawSkinMatrixIndex = 12u;
enum DepthBoneGpuMaxBones = 64u;
enum DepthBoneGpuMaxSources = 128u;
enum DepthBoneGpuMaxInfluences = 8u;
enum DepthBoneGpuMaxVertices = 1_000_000u;
enum DepthBoneGpuMaxStaleRetries = 3u;

private void depthBoneDebugLog(Args...)(const(char)[] fmt, Args args) {
    static if (EnableDepthBoneDebugLog) {
        import std.stdio : writefln;
        writefln(fmt, args);
    }
}

enum DepthBoneCommand {
    CreateDepthRigRoot,
    AddDepthBone,
    AddStandardDepthSkeleton,
    AddStandardDepthParameters,
    FitDepthRigRootZToDepth,
    FitDepthBoneZToDepth,
    SetDepthBoneRest,
    SetDepthBoneConstraint,
    ListDepthBones,
    AddDepthBoneSource,
    RemoveDepthBoneSource,
    ListDepthBoneSources,
    SetDepthBoneSourceSettings,
    SetDepthBoneInfluenceRule,
    GetDepthBoneInfluenceRule,
    PreviewDepthBoneInfluence,
    PreviewDepthBoneDeform,
    ApplyDepthBoneDeform,
}

shared static this() {
    ngDepthBoneMutationHook = &ngDepthBoneMutationChanged;
}

Command[DepthBoneCommand] commands;

private vec3 vec3From(float[] values, string name) {
    enforce(values.length == 3, _("%s must be [x, y, z]").format(name));
    return vec3(values[0], values[1], values[2]);
}

private ExDepthRigRoot requireRoot(Node node) {
    auto root = cast(ExDepthRigRoot)node;
    enforce(root !is null, "Node is not a DepthRigRoot");
    return root;
}

private ExDepthBone requireBone(Node node) {
    auto bone = cast(ExDepthBone)node;
    enforce(bone !is null, "Node is not a DepthBone");
    return bone;
}

private ExDepthTargetKind targetKindOf(Node node) {
    if (cast(GridDeformer)node) return ExDepthTargetKind.Grid;
    if (cast(PathDeformer)node) return ExDepthTargetKind.Path;
    enforce(false, "target must be GridDeformer or PathDeformer");
    assert(0);
}

private JSONValue boneToJson(ExDepthBone bone) {
    JSONValue[string] obj;
    obj["uuid"] = JSONValue(bone.uuid);
    obj["name"] = JSONValue(bone.name);
    obj["boneId"] = JSONValue(bone.boneId);
    obj["restHead"] = JSONValue([bone.restHead.x, bone.restHead.y, bone.restHead.z]);
    obj["restTail"] = JSONValue([bone.restTail.x, bone.restTail.y, bone.restTail.z]);
    obj["restRoll"] = JSONValue(bone.restRoll);
    obj["allowParentToTargets"] = JSONValue(bone.allowParentToTargets);
    if (bone.parent) obj["parent"] = JSONValue(bone.parent.uuid);
    return JSONValue(obj);
}

private JSONValue ruleToJson(ref ExDepthInfluenceRule rule) {
    JSONValue[string] obj;
    obj["maxInfluences"] = JSONValue(rule.maxInfluences);
    obj["radiusScale"] = JSONValue(rule.radiusScale);
    obj["minimumRadius"] = JSONValue(rule.minimumRadius);
    obj["falloff"] = JSONValue(rule.falloff);
    JSONValue[string] multipliers;
    foreach (uuid, value; rule.multipliersByBoneUuid) {
        multipliers[uuid.to!string] = JSONValue(value);
    }
    obj["multipliersByBoneUuid"] = JSONValue(multipliers);
    return JSONValue(obj);
}

private JSONValue sourceSettingsToJson(ExDepthBoneSourceSettings setting) {
    JSONValue[string] obj;
    obj["uuid"] = JSONValue(setting.boneUuid);
    obj["weight"] = JSONValue(setting.weight);
    obj["depthOffset"] = JSONValue(setting.depthOffset);
    obj["depthScale"] = JSONValue(setting.depthScale);
    obj["rotation"] = JSONValue(normalizeDepthBoneSourceRotation(setting.rotation));
    return JSONValue(obj);
}

private float jsonNumber(JSONValue value, float fallback) {
    final switch (value.type) {
        case JSONType.integer: return cast(float)value.integer;
        case JSONType.uinteger: return cast(float)value.uinteger;
        case JSONType.float_: return cast(float)value.floating;
        case JSONType.string: return value.str.length ? value.str.to!float : fallback;
        case JSONType.true_: return 1.0f;
        case JSONType.false_: return 0.0f;
        case JSONType.null_:
        case JSONType.object:
        case JSONType.array:
            return fallback;
    }
}

private float segmentLength(vec3 a, vec3 b) {
    auto d = b - a;
    return cast(float)sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

private vec3 normalizeVec(vec3 value, vec3 fallback = vec3(0, 1, 0)) {
    auto len = segmentLength(vec3(0, 0, 0), value);
    if (len <= 1e-8f) return fallback;
    return value / len;
}

private float dotVec(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

private bool isTerminalDepthBoneSource(ExDepthBone bone, ExDepthBone[] sources) {
    foreach (candidate; sources) {
        if (candidate.parent is bone) return false;
    }
    return true;
}

private quat quatFromUnitVectors(vec3 from, vec3 to) {
    from = normalizeVec(from);
    to = normalizeVec(to);
    auto r = dotVec(from, to) + 1.0f;
    vec3 axis;
    if (r < 1e-6f) {
        r = 0.0f;
        if (abs(from.x) > abs(from.z)) axis = vec3(-from.y, from.x, 0);
        else axis = vec3(0, -from.z, from.y);
    } else {
        axis = cross(from, to);
    }
    return quat(r, axis).normalized;
}

private mat4 composeMatrix(vec3 translation, quat rotation) {
    return mat4.translation(translation) * rotation.toMatrix!(4, 4);
}

private vec3 transformPoint(mat4 matrix, vec3 point) {
    auto result = matrix * vec4(point.x, point.y, point.z, 1.0f);
    return vec3(result.x, result.y, result.z);
}

private mat4 targetToRootMatrix(ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return mat4.identity;
    return root.transform.matrix.inverse * target.transform.matrix;
}

private ExDepthBone findBoneByUuid(ExDepthRigRoot root, ulong uuid) {
    foreach (bone; root.depthBones()) {
        if (bone.uuid == uuid) return bone;
    }
    return null;
}

private Node findNodeByUuid(Node root, ulong uuid) {
    if (root is null) return null;
    if (root.uuid == uuid) return root;
    foreach (child; root.children) {
        if (auto found = findNodeByUuid(child, uuid)) return found;
    }
    return null;
}

private Node findActiveNodeByUuid(ulong uuid) {
    auto puppet = incActivePuppet();
    if (puppet is null || puppet.root is null) return null;
    return findNodeByUuid(puppet.root, uuid);
}

private vec2 copyVertex2(Deformable target, size_t index) {
    auto vertex = target.vertices[index];
    return vec2(vertex.x, vertex.y);
}

private float targetBoundsSize(Deformable target) {
    if (target is null || target.vertices.length == 0) return 1.0f;
    auto minPoint = copyVertex2(target, 0);
    auto maxPoint = minPoint;
    foreach (i, vertex; target.vertices) {
        if (i == 0) continue;
        minPoint.x = min(minPoint.x, vertex.x);
        minPoint.y = min(minPoint.y, vertex.y);
        maxPoint.x = max(maxPoint.x, vertex.x);
        maxPoint.y = max(maxPoint.y, vertex.y);
    }
    auto size = maxPoint - minPoint;
    return max(1.0f, max(size.x, size.y));
}

private float depthWorldScale(ExDepthRigRoot root) {
    if (root is null || incActivePuppet() is null) return 1.0f;
    Deformable[] targets;
    foreach (ref binding; root.bindings) {
        auto target = cast(Deformable)incActivePuppet().find!Node(cast(uint)binding.targetUuid);
        if (target !is null) targets ~= target;
    }
    auto scale = ngDepthDisplayScaleForTargetsInNodeSpace(root, targets);
    return scale > 0.0f ? scale : 1.0f;
}

private float depthBoneSourceTangentSlope(float rotation) {
    auto normalized = normalizeDepthBoneSourceRotation(rotation);
    auto cosine = cast(float)cos(normalized);
    if (abs(cosine) < 1e-4f) cosine = cosine < 0.0f ? -1e-4f : 1e-4f;
    return cast(float)sin(normalized) / cosine;
}

private vec3 depthBoneSourceRestLocal(
    float x,
    float y,
    float rawDepth,
    ExDepthBoneSourceSettings setting,
    float depthUnitScale = 1.0f,
) {
    // Stored depth is the perpendicular origin-to-plane distance. The fitted
    // source line keeps Z=d and adds X=d*tan(rotation), giving D=d/cos(rotation).
    auto distance = (rawDepth * setting.depthScale + setting.depthOffset) * depthUnitScale;
    return vec3(
        x + distance * depthBoneSourceTangentSlope(setting.rotation),
        y,
        distance,
    );
}

private float depthBoneSourceRotationPivotXShift(
    Deformable target,
    const(float)[] scaledDepths,
    mat4 targetToRoot,
    vec3 boneRestHead,
    ExDepthBoneSourceSettings setting,
    float worldScale,
) {
    float planeDepth;
    float bestDistanceSq = float.infinity;
    bool found;
    foreach (i, vertex; target.vertices) {
        if (i >= scaledDepths.length) break;
        auto adjustedDepth = scaledDepths[i] * setting.depthScale
            + setting.depthOffset * worldScale;
        if (!adjustedDepth.isFinite) continue;
        auto sample = transformPoint(targetToRoot, vec3(vertex.x, vertex.y, adjustedDepth));
        if (!sample.x.isFinite || !sample.y.isFinite || !sample.z.isFinite) continue;
        auto dx = sample.x - boneRestHead.x;
        auto dy = sample.y - boneRestHead.y;
        auto distanceSq = dx * dx + dy * dy;
        if (!found || distanceSq < bestDistanceSq) {
            found = true;
            bestDistanceSq = distanceSq;
            planeDepth = sample.z;
        }
    }
    if (!found) {
        auto sample = transformPoint(targetToRoot, vec3(
            0.0f, 0.0f, setting.depthOffset * worldScale));
        planeDepth = sample.z.isFinite ? sample.z : 0.0f;
    }

    // Rotation changes one pivot for the whole source. Applying the same
    // X-axis shift to every vertex preserves relative relief while the
    // origin-to-plane line has angle R and length d/cos(R).
    auto planeDistance = planeDepth - boneRestHead.z;
    return planeDistance * depthBoneSourceTangentSlope(setting.rotation);
}

/**
 * Effective yaw pivot used by a BoneSource after applying its rotation setting.
 * Points are expressed in DepthRigRoot-local space at the current no-yaw pose.
 */
struct DepthBoneSourceEffectivePivot {
    ulong targetUuid;
    vec3 bonePoint;
    vec3 effectivePoint;
    float rotationPivotXShift;
    float rotation;
}

DepthBoneSourceEffectivePivot[] ngDepthBoneSourceEffectivePivots(
    ExDepthRigRoot root,
    ExDepthBone bone,
) {
    DepthBoneSourceEffectivePivot[] result;
    auto puppet = incActivePuppet();
    if (root is null || bone is null || puppet is null || puppet.root is null) return result;

    auto runtime = buildDepthRigRuntime(root);
    auto runtimeBone = bone.uuid in runtime;
    if (runtimeBone is null) return result;

    auto worldScale = depthWorldScale(root);
    auto noYawSkin = depthBoneNoYawSkinMatrix(*runtimeBone);
    auto bonePoint = transformPoint(noYawSkin, (*runtimeBone).restHead);

    foreach (ref binding; root.bindings) {
        if (binding.sourceBoneUuids.countUntil(bone.uuid) < 0) continue;

        auto targetNode = findNodeByUuid(puppet.root, binding.targetUuid);
        auto target = cast(Deformable)targetNode;
        if (target is null) continue;

        auto rawDepths = snapshotTargetDepths(target);
        float[] scaledDepths;
        scaledDepths.length = rawDepths.length;
        foreach (i, depth; rawDepths) scaledDepths[i] = depth * worldScale;
        auto targetToRoot = targetToRootMatrix(root, targetNode);

        auto setting = binding.sourceSetting(bone.uuid);
        auto shift = depthBoneSourceRotationPivotXShift(
            target,
            scaledDepths,
            targetToRoot,
            (*runtimeBone).restHead,
            setting,
            worldScale,
        );
        if (!shift.isFinite) continue;

        DepthBoneSourceEffectivePivot pivot;
        pivot.targetUuid = binding.targetUuid;
        pivot.bonePoint = bonePoint;
        pivot.effectivePoint = transformPoint(
            noYawSkin,
            (*runtimeBone).restHead - vec3(shift, 0.0f, 0.0f),
        );
        pivot.rotationPivotXShift = shift;
        pivot.rotation = setting.rotation;
        result ~= pivot;
    }
    return result;
}

private {
    ExDepthRigRoot depthBoneEffectivePivotCacheRoot;
    ExDepthBone depthBoneEffectivePivotCacheBone;
    DepthBoneSourceEffectivePivot[] depthBoneEffectivePivotCache;
    bool depthBoneEffectivePivotCacheDirty;
    bool depthBoneEffectivePivotCacheReady;
    ulong depthBoneEffectivePivotCacheGeneration;
    bool[ulong] depthBoneEffectivePivotDependencyUuids;
    Puppet depthBoneEffectivePivotObservedPuppet;

    final class DepthBoneEffectivePivotObserver {
        void onChange(Node target, NotifyReason reason) {
            if (target is null || !depthBoneEffectivePivotCacheReady) return;
            if (target.uuid in depthBoneEffectivePivotDependencyUuids ||
                isSameOrAncestorNode(depthBoneEffectivePivotCacheRoot, target)) {
                depthBoneEffectivePivotCacheDirty = true;
            }
        }
    }

    DepthBoneEffectivePivotObserver depthBoneEffectivePivotObserver;
}

private void clearDepthBoneEffectivePivotSelection() {
    depthBoneEffectivePivotCacheRoot = null;
    depthBoneEffectivePivotCacheBone = null;
    depthBoneEffectivePivotCache.length = 0;
    depthBoneEffectivePivotDependencyUuids.clear();
    depthBoneEffectivePivotCacheDirty = false;
    depthBoneEffectivePivotCacheReady = false;
}

private void syncDepthBoneEffectivePivotObserver() {
    auto puppet = incActivePuppet();
    if (puppet is depthBoneEffectivePivotObservedPuppet) return;

    if (depthBoneEffectivePivotObserver !is null &&
        depthBoneEffectivePivotObservedPuppet !is null &&
        depthBoneEffectivePivotObservedPuppet.root !is null) {
        depthBoneEffectivePivotObservedPuppet.root.removeNotifyListener(
            &depthBoneEffectivePivotObserver.onChange);
    }

    clearDepthBoneEffectivePivotSelection();
    depthBoneEffectivePivotObservedPuppet = puppet;
    if (depthBoneEffectivePivotObserver is null) {
        depthBoneEffectivePivotObserver = new DepthBoneEffectivePivotObserver;
    }
    if (puppet !is null && puppet.root !is null) {
        puppet.root.addNotifyListener(&depthBoneEffectivePivotObserver.onChange);
    }
}

private void addDepthBoneEffectivePivotDependency(Node node) {
    for (auto cursor = node; cursor !is null; cursor = cursor.parent) {
        depthBoneEffectivePivotDependencyUuids[cursor.uuid] = true;
    }
}

private void rebuildDepthBoneEffectivePivotDependencies() {
    depthBoneEffectivePivotDependencyUuids.clear();
    auto puppet = incActivePuppet();
    if (depthBoneEffectivePivotCacheRoot is null || puppet is null || puppet.root is null) return;

    addDepthBoneEffectivePivotDependency(depthBoneEffectivePivotCacheRoot);
    foreach (ref binding; depthBoneEffectivePivotCacheRoot.bindings) {
        addDepthBoneEffectivePivotDependency(
            findNodeByUuid(puppet.root, binding.targetUuid));
    }
}

private bool hasVisibleDepthBoneEffectivePivotSource(
    ExDepthRigRoot root,
    ExDepthBone bone,
) {
    if (root is null || bone is null) return false;
    foreach (ref binding; root.bindings) {
        if (binding.sourceBoneUuids.countUntil(bone.uuid) < 0) continue;
        if (normalizeDepthBoneSourceRotation(
            binding.sourceSetting(bone.uuid).rotation) != 0.0f) return true;
    }
    return false;
}

/**
 * Select the only BoneSource effective-pivot cache entry needed by ModelEdit.
 * Repeated calls with the same pair are constant-time and do not invalidate it.
 */
void ngSetDepthBoneEffectivePivotSelection(ExDepthRigRoot root, ExDepthBone bone) {
    syncDepthBoneEffectivePivotObserver();
    if (root is null || bone is null) {
        clearDepthBoneEffectivePivotSelection();
        return;
    }
    if (root is depthBoneEffectivePivotCacheRoot && bone is depthBoneEffectivePivotCacheBone) return;

    depthBoneEffectivePivotCacheRoot = root;
    depthBoneEffectivePivotCacheBone = bone;
    depthBoneEffectivePivotCache.length = 0;
    depthBoneEffectivePivotDependencyUuids.clear();
    depthBoneEffectivePivotCacheDirty = true;
    depthBoneEffectivePivotCacheReady = false;
}

/** Mark the selected overlay cache dirty without doing any geometry work. */
void ngInvalidateDepthBoneEffectivePivotCache(ExDepthRigRoot root = null) {
    if (depthBoneEffectivePivotCacheRoot is null) return;
    if (root is null || root is depthBoneEffectivePivotCacheRoot) {
        depthBoneEffectivePivotCacheDirty = true;
    }
}

/**
 * Process the coalesced overlay update outside viewport drawing. The numerical
 * result still comes from ngDepthBoneSourceEffectivePivots so rendering behavior
 * remains identical. Exact zero rotations are invisible and need no rig snapshot.
 */
void ngFlushDepthBoneEffectivePivotDirty() {
    syncDepthBoneEffectivePivotObserver();
    if (!depthBoneEffectivePivotCacheDirty) return;

    depthBoneEffectivePivotCacheDirty = false;
    depthBoneEffectivePivotCacheReady = false;
    depthBoneEffectivePivotCache.length = 0;
    auto root = depthBoneEffectivePivotCacheRoot;
    auto bone = depthBoneEffectivePivotCacheBone;
    if (root is null || bone is null || !isLiveDepthRigRoot(root) || !rootContainsBone(root, bone)) {
        clearDepthBoneEffectivePivotSelection();
        return;
    }

    rebuildDepthBoneEffectivePivotDependencies();
    if (hasVisibleDepthBoneEffectivePivotSource(root, bone)) {
        depthBoneEffectivePivotCache = ngDepthBoneSourceEffectivePivots(root, bone);
    }
    depthBoneEffectivePivotCacheReady = true;
    depthBoneEffectivePivotCacheGeneration++;
}

/** Return the last completed result. This function never rebuilds the rig. */
DepthBoneSourceEffectivePivot[] ngCachedDepthBoneSourceEffectivePivots(
    ExDepthRigRoot root,
    ExDepthBone bone,
) {
    syncDepthBoneEffectivePivotObserver();
    if (!depthBoneEffectivePivotCacheReady ||
        root !is depthBoneEffectivePivotCacheRoot ||
        bone !is depthBoneEffectivePivotCacheBone) return null;
    return depthBoneEffectivePivotCache;
}

bool ngHasPendingDepthBoneEffectivePivotRefresh() {
    return depthBoneEffectivePivotCacheDirty;
}

ulong ngDepthBoneEffectivePivotCacheGeneration() {
    return depthBoneEffectivePivotCacheGeneration;
}

private bool nearestScaledWorldDepthAtPoint(ExDepthRigRoot root, ExDepthBone bone, vec2 worldPoint, out float worldDepth) {
    auto puppet = incActivePuppet();
    if (root is null || puppet is null) {
        depthBoneDebugLog("[FitZ] no root or puppet root=%s puppet=%s", root is null, puppet is null);
        return false;
    }

    size_t sampleCount = 0;
    float totalDepth = 0.0f;
    auto worldScale = depthWorldScale(root);
    depthBoneDebugLog("[FitZ] start root=%s bone=%s bindings=%s", root.name, bone is null ? "(fallback)" : bone.name, root.bindings.length);
    foreach (ref binding; root.bindings) {
        ExDepthBoneSourceSettings setting;
        if (bone !is null) {
            if (binding.sourceBoneUuids.countUntil(bone.uuid) < 0) {
                depthBoneDebugLog("[FitZ] skip binding target=%s bone uuid=%s not in sources", binding.targetUuid, bone.uuid);
                continue;
            }
            setting = binding.sourceSetting(bone.uuid);
        }

        auto targetNode = findNodeByUuid(puppet.root, binding.targetUuid);
        auto target = cast(Deformable)targetNode;
        auto mapped = cast(DepthMappedNode)targetNode;
        if (targetNode is null || target is null || mapped is null) {
            depthBoneDebugLog("[FitZ] skip target=%s nodeNull=%s deformableNull=%s mappedNull=%s",
                binding.targetUuid, targetNode is null, target is null, mapped is null);
            continue;
        }

        auto depths = mapped.copyDepths();
        if (depths is null || depths.length == 0) {
            depthBoneDebugLog("[FitZ] skip target=%s depths=%s", targetNode.name, depths is null ? -1 : cast(int)depths.length);
            continue;
        }
        depthBoneDebugLog("[FitZ] target=%s vertices=%s depths=%s", targetNode.name, target.vertices.length, depths.length);

        auto targetToWorld = targetNode.transform.matrix;
        bool foundTargetSample = false;
        float bestDistanceSq = float.max;
        float bestDepth = 0.0f;
        if (target.vertices.length > 0) {
            foreach (i, vertex; target.vertices) {
                if (i >= depths.length) break;
                auto sourceLocal = depthBoneSourceRestLocal(
                    vertex.x, vertex.y, depths[i], setting, worldScale);
                if (!sourceLocal.x.isFinite || !sourceLocal.y.isFinite || !sourceLocal.z.isFinite) continue;
                auto sample = transformPoint(targetToWorld, sourceLocal);
                if (!sample.x.isFinite || !sample.y.isFinite || !sample.z.isFinite) {
                    sample = sourceLocal;
                }
                auto dx = sample.x - worldPoint.x;
                auto dy = sample.y - worldPoint.y;
                auto distanceSq = dx * dx + dy * dy;
                if (!foundTargetSample || distanceSq < bestDistanceSq) {
                    foundTargetSample = true;
                    bestDistanceSq = distanceSq;
                    bestDepth = sample.z;
                }
            }
            depthBoneDebugLog("[FitZ] scanned vertices found=%s bestDepth=%s bestDistanceSq=%s", foundTargetSample, bestDepth, bestDistanceSq);
        } else {
            float totalTargetDepth = 0.0f;
            size_t targetDepthCount = 0;
            foreach (i, value; depths) {
                if (!value.isFinite) continue;
                totalTargetDepth += value;
                targetDepthCount++;
            }
            if (targetDepthCount > 0) {
                auto sourceLocal = depthBoneSourceRestLocal(
                    0.0f,
                    0.0f,
                    totalTargetDepth / cast(float)targetDepthCount,
                    setting,
                    worldScale,
                );
                auto sample = transformPoint(targetToWorld, sourceLocal);
                if (!sample.z.isFinite) sample = vec3(worldPoint.x, worldPoint.y, sourceLocal.z);
                foundTargetSample = true;
                bestDepth = sample.z;
            }
        }

        if (foundTargetSample) {
            totalDepth += bestDepth;
            sampleCount++;
            depthBoneDebugLog("[FitZ] accepted target=%s bestDepth=%s sampleCount=%s", targetNode.name, bestDepth, sampleCount);
        }
    }

    if (sampleCount == 0) {
        depthBoneDebugLog("[FitZ] no accepted samples");
        return false;
    }
    worldDepth = totalDepth / cast(float)sampleCount;
    return true;
}

bool ngDepthRigNodeCurrentScaledDepth(Node node, out float localTranslationZ, out float rootDepth) {
    if (auto root = cast(ExDepthRigRoot)node) {
        foreach (bone; root.depthBones()) {
            if (ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) return true;
        }
        return false;
    }

    auto bone = cast(ExDepthBone)node;
    if (bone is null) return false;
    auto root = findDepthRigRoot(bone);
    if (root is null) return false;

    auto worldPoint = bone.transform.translation.xy;
    if (!nearestScaledWorldDepthAtPoint(root, bone, worldPoint, rootDepth)
        && !nearestScaledWorldDepthAtPoint(root, null, worldPoint, rootDepth)) {
        return false;
    }
    if (!rootDepth.isFinite) {
        depthBoneDebugLog("[FitZ] rootDepth not finite bone=%s rootDepth=%s", bone.name, rootDepth);
        return false;
    }
    auto currentWorldZ = bone.transform.translation.vector[2];
    if (!currentWorldZ.isFinite) {
        depthBoneDebugLog("[FitZ] currentWorldZ not finite bone=%s currentWorldZ=%s", bone.name, currentWorldZ);
        return false;
    }
    auto currentLocalZ = bone.localTransform.translation.vector[2];
    localTranslationZ = currentLocalZ + (rootDepth - currentWorldZ);
    if (!localTranslationZ.isFinite) {
        depthBoneDebugLog("[FitZ] localTranslationZ not finite bone=%s local=%s localZRead=%s rootDepth=%s currentWorldZ=%s",
            bone.name, localTranslationZ, currentLocalZ, rootDepth, currentWorldZ);
        return false;
    }
    return true;
}

size_t ngDepthRigRootFittableDepthBoneCount(ExDepthRigRoot root) {
    if (root is null) return 0;
    size_t result;
    foreach (bone; root.depthBones()) {
        float localTranslationZ;
        float rootDepth;
        if (ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) result++;
    }
    return result;
}

private bool fitDepthBoneTranslationZToCurrentDepth(ExDepthRigRoot root, ExDepthBone bone, GroupAction group, ref bool changed) {
    if (root is null || bone is null) return false;

    float localTranslationZ;
    float rootDepth;
    if (!ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) {
        depthBoneDebugLog("[FitZ] current scaled depth failed bone=%s", bone.name);
        return false;
    }
    depthBoneDebugLog("[FitZ] fit translation bone=%s localZ=%s rootDepth=%s", bone.name, localTranslationZ, rootDepth);
    return fitDepthBoneTranslationZ(root, bone, localTranslationZ, group, changed);
}

private bool fitDepthBoneTranslationZ(ExDepthRigRoot root, ExDepthBone bone, float localTranslationZ, GroupAction group, ref bool changed) {
    if (root is null || bone is null || !localTranslationZ.isFinite) return false;

    auto oldValue = bone.localTransform.translation.vector[2];
    depthBoneDebugLog("[FitZ] apply translation bone=%s old=%s new=%s", bone.name, oldValue, localTranslationZ);
    if (abs(oldValue - localTranslationZ) <= 0.00001f) return true;
    bone.localTransform.translation.vector[2] = localTranslationZ;
    bone.localTransform.update();
    bone.transformChanged();

    auto action = new NodeValueChangeAction!(Node, float)(
        "translationZ",
        bone,
        oldValue,
        localTranslationZ,
        &bone.localTransform.translation.vector[2]
    );
    if (group !is null) group.addAction(action);
    else incActionPush(action);
    changed = true;
    return true;
}

private Parameter depthFitParameter(ExDepthRigRoot root) {
    auto param = incArmedParameter();
    if (param !is null) return param;
    if (lastDepthBoneDirtyRoot is root && lastDepthBoneDirtyParameter !is null)
        return lastDepthBoneDirtyParameter;
    return null;
}

private bool fitDepthBoneBindingZToCurrentDepth(
    ExDepthRigRoot root,
    ExDepthBone bone,
    Parameter param,
    vec2u kp,
    GroupAction group,
    ref bool changed,
) {
    if (root is null || bone is null || param is null) return false;

    float bindingZ;
    float worldDepth;
    auto worldPoint = bone.transform.translation.xy;
    if (!nearestScaledWorldDepthAtPoint(root, bone, worldPoint, worldDepth)
        && !nearestScaledWorldDepthAtPoint(root, null, worldPoint, worldDepth)) {
        return false;
    }
    if (!worldDepth.isFinite || !bone.transform.translation.vector[2].isFinite) return false;
    bindingZ = worldDepth - bone.transform.translation.vector[2];
    if (!bindingZ.isFinite) return false;

    auto binding = cast(ValueParameterBinding)param.getBinding(bone, "transform.t.z");
    if (binding is null) {
        binding = cast(ValueParameterBinding)param.createBinding(bone, "transform.t.z");
        if (binding is null) return false;
        if (group !is null) group.addAction(new ParameterBindingAddAction(param, binding));
    }

    auto oldValue = binding.getValue(kp);
    if (abs(oldValue - bindingZ) <= 0.00001f) return true;

    auto action = new ParameterBindingValueChangeAction!(float, ValueParameterBinding)(binding.getName(), binding, kp.x, kp.y);
    binding.setValue(kp, bindingZ);
    action.updateNewState();
    if (group !is null) group.addAction(action);
    else incActionPush(action);
    changed = true;
    return true;
}

private bool fitOneDepthBoneZToDepth(
    ExDepthRigRoot root,
    ExDepthBone bone,
    GroupAction group,
    ref bool changed,
) {
    return fitDepthBoneTranslationZToCurrentDepth(root, bone, group, changed);
}

bool ngFitDepthRigNodeTranslationZToCurrentDepth(Node node) {
    if (auto root = cast(ExDepthRigRoot)node) {
        auto group = new GroupAction();
        bool changed;
        bool fitted;
        foreach (bone; root.depthBones()) {
            fitted = fitOneDepthBoneZToDepth(root, bone, group, changed) || fitted;
        }
        if (!fitted) {
            incSetStatus(_("Fit Z to Depth failed: no usable depth samples were found."));
            return false;
        }
        if (changed) {
            incActionPush(group);
        }
        incSetStatus(changed
            ? _("Fit Z to Depth updated descendant DepthBone translation.t.z values.")
            : _("Fit Z to Depth completed; descendant DepthBone translation.t.z values were already aligned."));
        return true;
    }

    if (auto bone = cast(ExDepthBone)node) {
        auto root = findDepthRigRoot(bone);
        if (root is null) return false;
        auto group = new GroupAction();
        bool changed;
        if (!fitOneDepthBoneZToDepth(root, bone, group, changed)) {
            incSetStatus(_("Fit Z to Depth failed: no usable depth sample was found for the DepthBone."));
            return false;
        }
        if (changed) incActionPush(group);
        incSetStatus(changed
            ? _("Fit Z to Depth updated the DepthBone translation.t.z value.")
            : _("Fit Z to Depth completed; the DepthBone translation.t.z value was already aligned."));
        return true;
    }
    return false;
}

private quat depthEditRotation(float pitch, float yaw, float roll) {
    return quat.eulerRotation(-pitch, -yaw, roll);
}

private vec3 depthBoneNodeRestPosition(ExDepthRigRoot root, Node node) {
    vec3 result = vec3(0, 0, 0);
    Node cursor = node;
    while (cursor !is null && cursor !is root) {
        result += cursor.localTransform.translation;
        if (cursor.lockToRoot) break;
        cursor = cursor.parent;
    }
    return result;
}

private ExDepthBone firstDepthBoneChild(ExDepthBone bone) {
    foreach (child; bone.children) {
        if (auto childBone = cast(ExDepthBone)child) return childBone;
    }
    return null;
}

private vec3 depthBonePlanarDirection(vec3 direction) {
    direction.z = 0;
    if (segmentLength(vec3(0, 0, 0), direction) > 1e-4f) return direction;
    return vec3(0, 0, 0);
}

private void effectiveDepthBoneRest(ExDepthRigRoot root, ExDepthBone bone, out vec3 head, out vec3 tail) {
    head = depthBoneNodeRestPosition(root, bone);
    if (auto childBone = firstDepthBoneChild(bone)) {
        auto direction = depthBonePlanarDirection(depthBoneNodeRestPosition(root, childBone) - head);
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f)
            direction = depthBonePlanarDirection(bone.restTail - bone.restHead);
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f)
            direction = vec3(0, 100, 0);
        tail = head + direction;
        return;
    }
    if (auto parentBone = cast(ExDepthBone)bone.parent) {
        auto parentPoint = depthBoneNodeRestPosition(root, parentBone);
        auto direction = depthBonePlanarDirection(head - parentPoint);
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f)
            direction = depthBonePlanarDirection(bone.restTail - bone.restHead);
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f) direction = vec3(0, 100, 0);
        tail = head + direction;
        return;
    }

    auto fallback = depthBonePlanarDirection(bone.restTail - bone.restHead);
    if (segmentLength(vec3(0, 0, 0), fallback) <= 1e-4f) fallback = vec3(0, 100, 0);
    tail = head + fallback;
}

private class RuntimeDepthBone {
    ExDepthBone source;
    RuntimeDepthBone parent;
    vec3 restHead;
    vec3 restTail;
    float restLength;
    quat restQuaternion;
    quat localRestQuaternion;
    vec3 localRestOffset;
    vec3 poseTranslation;
    quat poseQuaternion;
    quat poseWithoutYawQuaternion;
    float poseYaw;
    vec3 worldHead;
    vec3 worldTail;
    quat worldQuaternion;
    quat worldPosePrefix;
    mat4 bindMatrix;
    mat4 inverseBindMatrix;
    mat4 skinMatrix;
}

struct DepthBoneGpuOffsetPacket {
    ExDepthRigRoot root;
    Deformable target;
    Parameter parameter;
    vec2u keypoint;
    bool writePreview;
    bool writeBinding;
    Vec2Array vertices;
    float[] rawDepths;
    float[] depths;
    float[] bones;
    float[] sourceInputs;
    float[] sources;
    mat4 targetToRoot;
    mat4 rootToTarget;
    float worldScale;
    float influenceRadiusFloor;
    float radiusScale;
    uint boneCount;
    uint sourceCount;
    uint maxInfluences;
    ulong rigHash;
    ulong parameterStructureHash;
    ulong poseHash;
    uint staleRetryCount;

    DepthBoneGpuDispatchPacket dispatchPacket() {
        DepthBoneGpuDispatchPacket packet;
        packet.vertices = vertices;
        packet.depths = depths;
        packet.bones = bones;
        packet.sources = sources;
        packet.targetToRoot = targetToRoot;
        packet.rootToTarget = rootToTarget;
        packet.influenceRadiusFloor = influenceRadiusFloor;
        packet.radiusScale = radiusScale;
        packet.boneCount = boneCount;
        packet.sourceCount = sourceCount;
        packet.maxInfluences = maxInfluences;
        return packet;
    }
}

private float[] snapshotTargetDepths(Deformable target) {
    float[] result;
    if (target is null) return result;
    result.length = target.vertices.length;
    auto mapped = cast(DepthMappedNode)target;
    if (mapped is null) return result;
    auto stored = mapped.copyDepths();
    auto count = min(result.length, stored.length);
    if (count > 0) result[0 .. count] = stored[0 .. count];
    return result;
}

private float parameterValue(Node node, Parameter param, vec2u cursor, string key, float fallback) {
    if (node is null || param is null) return fallback;
    if (auto binding = cast(ValueParameterBinding)param.getBinding(node, key)) {
        return binding.getValue(cursor);
    }
    return fallback;
}

private RuntimeDepthBone[ulong] buildDepthRigRuntime(ExDepthRigRoot root, Parameter param = null, vec2u cursor = vec2u.init) {
    enforce(root !is null, "Depth rig root is required");
    RuntimeDepthBone[ulong] runtime;
    auto upAxis = vec3(0, 1, 0);

    foreach (bone; root.depthBones()) {
        auto rb = new RuntimeDepthBone();
        rb.source = bone;
        effectiveDepthBoneRest(root, bone, rb.restHead, rb.restTail);
        rb.restLength = segmentLength(bone.restHead, bone.restTail);
        rb.restLength = segmentLength(rb.restHead, rb.restTail);
        if (rb.restLength <= 1e-4f) rb.restLength = 1e-4f;
        rb.restQuaternion = quatFromUnitVectors(upAxis, rb.restTail - rb.restHead) * quat.axisRotation(bone.restRoll, upAxis);
        rb.bindMatrix = composeMatrix(rb.restHead, rb.restQuaternion);
        rb.inverseBindMatrix = rb.bindMatrix.inverse;
        if (param !is null) {
            rb.poseTranslation = bone.lockTranslation
                ? vec3(0, 0, 0)
                : vec3(
                    parameterValue(bone, param, cursor, "transform.t.x", 0),
                    parameterValue(bone, param, cursor, "transform.t.y", 0),
                    parameterValue(bone, param, cursor, "transform.t.z", 0)
                );
            auto pitch = parameterValue(bone, param, cursor, "transform.r.x", 0);
            auto yaw = parameterValue(bone, param, cursor, "transform.r.y", 0);
            auto roll = parameterValue(bone, param, cursor, "transform.r.z", 0);
            rb.poseYaw = bone.lockRotation ? 0.0f : yaw;
            rb.poseWithoutYawQuaternion = bone.lockRotation
                ? quat.identity
                : depthEditRotation(pitch, 0.0f, roll);
            rb.poseQuaternion = bone.lockRotation
                ? quat.identity
                : depthEditRotation(pitch, yaw, roll);
        } else {
            vec3 restLocalTranslation;
            if (auto parentBone = cast(ExDepthBone)bone.parent) {
                auto parentRest = parentBone.uuid in runtime;
                restLocalTranslation = parentRest is null ? rb.restHead : rb.restHead - (*parentRest).restHead;
            } else {
                restLocalTranslation = rb.restHead;
            }
            rb.poseTranslation = bone.lockTranslation ? vec3(0, 0, 0) : bone.localTransform.translation - restLocalTranslation;
            rb.poseYaw = bone.lockRotation ? 0.0f : bone.localTransform.rotation.y;
            rb.poseWithoutYawQuaternion = bone.lockRotation
                ? quat.identity
                : depthEditRotation(
                    bone.localTransform.rotation.x,
                    0.0f,
                    bone.localTransform.rotation.z,
                );
            rb.poseQuaternion = bone.lockRotation
                ? quat.identity
                : depthEditRotation(
                    bone.localTransform.rotation.x,
                    bone.localTransform.rotation.y,
                    bone.localTransform.rotation.z,
                );
        }
        runtime[bone.uuid] = rb;
    }

    foreach (bone; root.depthBones()) {
        auto rb = runtime[bone.uuid];
        if (auto parentBone = cast(ExDepthBone)bone.parent) {
            if (auto parent = parentBone.uuid in runtime) {
                rb.parent = *parent;
                rb.localRestQuaternion = rb.parent.restQuaternion.inverse * rb.restQuaternion;
                rb.localRestOffset = rb.parent.restQuaternion.inverse * (rb.restHead - rb.parent.restHead);
            } else {
                rb.localRestQuaternion = rb.restQuaternion;
                rb.localRestOffset = rb.restHead;
            }
        } else {
            rb.localRestQuaternion = rb.restQuaternion;
            rb.localRestOffset = rb.restHead;
        }
    }

    foreach (bone; root.depthBones()) {
        auto rb = runtime[bone.uuid];
        if (rb.parent !is null && bone.allowParentToTargets && !bone.lockToRoot) {
            rb.worldHead = rb.parent.worldHead + (rb.parent.worldQuaternion * rb.localRestOffset) + (rb.parent.worldQuaternion * rb.poseTranslation);
            rb.worldPosePrefix = rb.parent.worldQuaternion * rb.localRestQuaternion;
        } else {
            rb.worldHead = rb.restHead + rb.poseTranslation;
            rb.worldPosePrefix = rb.restQuaternion;
        }
        rb.worldQuaternion = rb.worldPosePrefix * rb.poseQuaternion;
        rb.worldTail = rb.worldHead + (rb.worldQuaternion * vec3(0, rb.restLength, 0));
        rb.skinMatrix = composeMatrix(rb.worldHead, rb.worldQuaternion) * rb.inverseBindMatrix;
    }

    return runtime;
}

private mat4 depthBoneNoYawSkinMatrix(RuntimeDepthBone bone) {
    if (bone is null) return mat4.identity;
    if (bone.poseYaw == 0.0f) return bone.skinMatrix;
    auto sourceWorld = bone.worldPosePrefix * bone.poseWithoutYawQuaternion;
    return composeMatrix(bone.worldHead, sourceWorld) * bone.inverseBindMatrix;
}

private void appendVec3(ref float[] values, vec3 value) {
    values ~= value.x;
    values ~= value.y;
    values ~= value.z;
}

private void appendMat4(ref float[] values, mat4 value) {
    foreach (i; 0 .. 16) values ~= value.ptr[i];
}

bool ngDepthBoneGpuSupported() {
    return ngDepthBoneGpuAsyncSupported();
}

string ngDepthBoneGpuSupportDiagnostic() {
    if (ngDepthBoneGpuAsyncSupported()) return "Depth bone GPU async deformation is supported";
    auto missing = ngDepthBoneGpuAsyncMissingRequirements();
    return "Depth bone GPU async deformation is unavailable; missing: " ~ missing.join(", ");
}

private void writeDepthBoneGpuFatalLog(string message) {
    try {
        import std.datetime : Clock;
        import std.file : append;
        import std.path : buildPath;
        import std.process : environment;

        auto dir = environment.get("TEMP", ".");
        append(buildPath(dir, "nijigenerate-depthbone-gpu.log"),
            "[%s] %s\n".format(Clock.currTime.toISOString(), message));
    } catch (Exception) {
    }
}

private void enforceDepthBoneGpuAvailable(string context) {
    if (ngDepthBoneGpuSupported()) return;
    auto message = "Depth Bone GPU unavailable for %s: %s".format(context, ngDepthBoneGpuSupportDiagnostic());
    writeDepthBoneGpuFatalLog(message);
    enforce(false, message);
}

Vec2Array ngDepthBoneGpuReadbackToOffsets(float[] xs, float[] ys) {
    enforce(xs.length == ys.length, "Depth bone GPU readback arrays must have the same length");
    Vec2Array offsets;
    offsets.length = xs.length;
    foreach (i; 0 .. xs.length) offsets[i] = vec2(xs[i], ys[i]);
    return offsets;
}

bool ngBuildDepthBoneGpuOffsetPacket(
    ExDepthRigRoot root,
    ExDepthRigBinding* binding,
    Deformable target,
    Parameter param,
    vec2u cursor,
    out DepthBoneGpuOffsetPacket packet,
    out string error,
    bool writePreview = false,
    bool writeBinding = true
) {
    packet = DepthBoneGpuOffsetPacket.init;
    error = null;
    if (root is null) {
        error = "Depth rig root is required";
        return false;
    }
    if (binding is null) {
        error = "Depth rig binding is required";
        return false;
    }
    if (target is null) {
        error = "target is not deformable";
        return false;
    }
    if (binding.sourceBoneUuids.length == 0) {
        error = "Depth rig binding has no bone sources";
        return false;
    }
    if (target.vertices.length > DepthBoneGpuMaxVertices) {
        error = "Depth bone GPU packet exceeds maximum vertex count";
        return false;
    }

    ExDepthBone[] sourceBones;
    foreach (uuid; binding.sourceBoneUuids) {
        if (auto bone = findBoneByUuid(root, uuid)) sourceBones ~= bone;
    }
    if (sourceBones.length == 0) {
        error = "No valid depth bone sources";
        return false;
    }

    auto runtime = buildDepthRigRuntime(root, param, cursor);
    uint[ulong] boneIndices;
    ExDepthBone[] runtimeBones;
    foreach (bone; root.depthBones()) {
        if (bone.uuid !in runtime) continue;
        if (runtimeBones.length >= DepthBoneGpuMaxBones) {
            error = "Depth bone GPU packet exceeds maximum bone count";
            return false;
        }
        boneIndices[bone.uuid] = cast(uint)runtimeBones.length;
        runtimeBones ~= bone;
    }
    if (sourceBones.length > DepthBoneGpuMaxSources) {
        error = "Depth bone GPU packet exceeds maximum source count";
        return false;
    }
    auto maxInfluences = binding.influenceRule.maxInfluences == 0 ? 1 : binding.influenceRule.maxInfluences;
    if (maxInfluences > DepthBoneGpuMaxInfluences) {
        error = "Depth bone GPU packet exceeds maximum influence count";
        return false;
    }

    float[] boneData;
    foreach (bone; runtimeBones) {
        auto runtimeBone = bone.uuid in runtime;
        if (runtimeBone is null) continue;
        appendVec3(boneData, (*runtimeBone).restHead);
        boneData ~= (*runtimeBone).restLength;
        appendVec3(boneData, (*runtimeBone).restTail);
        float parentIndex = -1.0f;
        if ((*runtimeBone).parent !is null && (*runtimeBone).parent.source !is null) {
            if (auto parent = (*runtimeBone).parent.source.uuid in boneIndices) parentIndex = cast(float)*parent;
        }
        boneData ~= parentIndex;
        appendMat4(boneData, (*runtimeBone).skinMatrix);
    }

    auto worldScale = depthWorldScale(root);
    auto rawDepths = snapshotTargetDepths(target);
    float[] scaledDepths;
    scaledDepths.length = rawDepths.length;
    foreach (i, depth; rawDepths) scaledDepths[i] = depth * worldScale;
    auto targetNode = cast(Node)target;
    auto targetToRoot = targetToRootMatrix(root, targetNode);

    float[] sourceData;
    float[] sourceInputs;
    foreach (bone; sourceBones) {
        auto boneIndex = bone.uuid in boneIndices;
        auto runtimeBone = bone.uuid in runtime;
        if (boneIndex is null || runtimeBone is null) {
            error = "Depth bone runtime is missing";
            return false;
        }
        auto setting = binding.sourceSetting(bone.uuid);
        auto multiplier = 1.0f;
        if (auto p = bone.uuid in binding.influenceRule.multipliersByBoneUuid) multiplier = *p;
        auto sourceStart = sourceInputs.length;
        sourceInputs ~= cast(float)*boneIndex;
        sourceInputs ~= (isTerminalDepthBoneSource(bone, sourceBones) ? 1.0f : 0.0f);
        sourceInputs ~= (bone.lockToRoot ? 1.0f : 0.0f);
        sourceInputs ~= (binding.influenceRule.falloff == "linear" ? 1.0f : 0.0f);
        sourceInputs ~= setting.weight;
        sourceInputs ~= setting.depthScale;
        sourceInputs ~= setting.depthOffset;
        sourceInputs ~= multiplier;
        auto rotation = normalizeDepthBoneSourceRotation(setting.rotation);
        sourceInputs ~= cast(float)sin(rotation);
        sourceInputs ~= cast(float)cos(rotation);
        auto sourceDataStart = sourceData.length;
        sourceData ~= sourceInputs[sourceStart .. $];
        sourceData[sourceDataStart + DepthBoneGpuSourceDepthOffsetIndex] *= worldScale;
        sourceData ~= depthBoneSourceRotationPivotXShift(
            target,
            scaledDepths,
            targetToRoot,
            (*runtimeBone).restHead,
            setting,
            worldScale,
        );
        sourceData ~= (*runtimeBone).poseYaw;
        appendMat4(sourceData, depthBoneNoYawSkinMatrix(*runtimeBone));
    }

    packet.root = root;
    packet.target = target;
    packet.parameter = param;
    packet.keypoint = cursor;
    packet.writePreview = writePreview;
    packet.writeBinding = writeBinding;
    packet.vertices = target.vertices.dup;
    packet.rawDepths = rawDepths;
    packet.depths = scaledDepths;
    packet.targetToRoot = targetToRoot;
    packet.rootToTarget = packet.targetToRoot.inverse;
    packet.worldScale = worldScale;
    packet.influenceRadiusFloor = max(binding.influenceRule.minimumRadius, targetBoundsSize(target) * 0.18f);
    packet.radiusScale = binding.influenceRule.radiusScale;
    packet.maxInfluences = maxInfluences;
    packet.bones = boneData;
    packet.sourceInputs = sourceInputs;
    packet.sources = sourceData;
    packet.boneCount = cast(uint)runtimeBones.length;
    packet.sourceCount = cast(uint)sourceBones.length;
    packet.rigHash = depthBoneRigStructureHash(root);
    packet.parameterStructureHash = param is null ? 0 : depthBoneParameterStructureHash(root, param);
    packet.poseHash = param is null ? 0 : depthBonePoseKeyHash(root, param, cursor);
    return true;
}

private Vec2Array generateInfluencePreviewOffsets(ExDepthRigBinding* binding, ExDepthBone bone, Deformable target) {
    enforce(binding !is null, "Depth rig binding is required");
    enforce(bone !is null, "Depth bone is required");
    enforce(target !is null, "target is not deformable");

    Vec2Array offsets;
    offsets.length = target.vertices.length;
    auto radius = segmentLength(bone.restHead, bone.restTail) * binding.influenceRule.radiusScale;
    if (radius < binding.influenceRule.minimumRadius) radius = binding.influenceRule.minimumRadius;
    if (radius <= 1e-6f) radius = 1.0f;
    foreach (i, vertex; target.vertices) {
        offsets[i] = vec2(0, radius * 0.2f);
    }
    return offsets;
}

private bool hasValidDepthBoneSources(ExDepthRigRoot root, ref ExDepthRigBinding binding) {
    if (root is null || binding.sourceBoneUuids.length == 0) return false;
    foreach (uuid; binding.sourceBoneUuids) {
        if (findBoneByUuid(root, uuid) !is null) return true;
    }
    return false;
}

private ExDepthRigRoot findDepthRigRoot(ExDepthBone bone) {
    Node cursor = bone;
    while (cursor !is null) {
        if (auto root = cast(ExDepthRigRoot)cursor) return root;
        cursor = cursor.parent;
    }
    return null;
}

private bool isSameOrDescendantBone(ExDepthBone bone, ExDepthBone ancestor) {
    Node cursor = bone;
    while (cursor !is null) {
        if (cursor is ancestor) return true;
        cursor = cursor.parent;
    }
    return false;
}

enum DepthBoneDirtyScope {
    Keypoint,
    AllKeypoints,
}

private struct DepthBoneDirtyRequest {
    ExDepthRigRoot root;
    Parameter parameter;
    vec2u keypoint;
    uint targetUuid;
    DepthBoneDirtyScope dirtyScope;
    string reason;
    GroupAction actionSink;
    bool settleBeforeDispatch;
}

private DepthBoneDirtyRequest[] depthBoneDirtyRequests;
private ExDepthRigRoot lastDepthBoneDirtyRoot;
private Parameter lastDepthBoneDirtyParameter;
private vec2u lastDepthBoneDirtyKeypoint;
private GroupAction depthBoneRefreshActionSink;

private enum size_t DepthBoneAllKeypointsPerFrame = 4;
private enum size_t DepthBoneGpuSubmissionsPerFrame = 8;
private enum size_t DepthBoneGpuReadbacksPerFrame = 2;
private enum size_t DepthBoneGpuCompletedBatchesPerFrame = 1;
private enum size_t DepthBoneGpuMaxInFlight = 8;

private struct DepthBoneAllKeypointJob {
    ExDepthRigRoot root;
    Parameter parameter;
    uint targetUuid;
    vec2u[] keypoints;
    bool[string] processed;
    size_t nextIndex;
    string reason;
    GroupAction actionSink;
    size_t settleFrames;
}

private DepthBoneAllKeypointJob[] depthBoneAllKeypointJobs;

private struct DepthBoneGpuRefreshJob {
    uint jobId;
    DepthBoneGpuOffsetPacket packet;
    string reason;
    GroupAction actionSink;
    uint batchId;
    size_t batchExpected;
}

private struct DepthBoneGpuQueuedJob {
    DepthBoneGpuOffsetPacket packet;
    string reason;
    GroupAction actionSink;
    uint batchId;
    size_t batchExpected;
}

private struct DepthBoneGpuCompletedJob {
    DepthBoneGpuOffsetPacket packet;
    string reason;
    GroupAction actionSink;
    uint batchId;
    size_t batchExpected;
    Vec2Array offsets;
    bool valid;
}

private DepthBoneGpuQueuedJob[] depthBoneGpuSubmissionQueue;
private DepthBoneGpuRefreshJob[] depthBoneGpuRefreshJobs;
private DepthBoneGpuCompletedJob[] depthBoneGpuCompletedJobs;
private bool[uint] canceledDepthBoneGpuBatches;
private uint nextDepthBoneGpuBatchId = 1;

void ngBeginDepthBoneRefreshActionSink(GroupAction sink) {
    depthBoneRefreshActionSink = sink;
}

void ngEndDepthBoneRefreshActionSink(GroupAction sink) {
    if (depthBoneRefreshActionSink is sink) depthBoneRefreshActionSink = null;
}

private void pushDepthBoneRefreshAction(GroupAction group) {
    if (group is null || group.empty()) return;
    if (depthBoneRefreshActionSink !is null) {
        depthBoneRefreshActionSink.addAction(group);
    } else {
        incActionPush(group);
    }
}

private bool runWithDepthBoneRefreshActionSink(GroupAction sink, bool delegate() callback) {
    auto previous = depthBoneRefreshActionSink;
    depthBoneRefreshActionSink = sink;
    scope(exit) depthBoneRefreshActionSink = previous;
    return callback();
}

private bool submitDepthBoneGpuRefreshJob(DepthBoneGpuQueuedJob queued, out string error) {
    auto packet = queued.packet;
    auto dispatch = packet.dispatchPacket();
    uint jobId;
    if (!ngSubmitDepthBoneGpuAsync(dispatch, jobId, error)) return false;
    depthBoneGpuRefreshJobs ~= DepthBoneGpuRefreshJob(
        jobId, packet, queued.reason, queued.actionSink, queued.batchId, queued.batchExpected);
    return true;
}

private void enqueueDepthBoneGpuRefreshBatch(DepthBoneGpuOffsetPacket[] packets, string reason, GroupAction actionSink) {
    if (packets.length == 0) return;
    auto batchId = nextDepthBoneGpuBatchId++;
    if (nextDepthBoneGpuBatchId == 0) nextDepthBoneGpuBatchId = 1;
    foreach (packet; packets) {
        depthBoneGpuSubmissionQueue ~= DepthBoneGpuQueuedJob(
            packet, reason, actionSink, batchId, packets.length);
    }
}

private void completeDepthBoneGpuJob(DepthBoneGpuRefreshJob job, Vec2Array offsets, bool valid) {
    depthBoneGpuCompletedJobs ~= DepthBoneGpuCompletedJob(
        job.packet, job.reason, job.actionSink, job.batchId, job.batchExpected, offsets, valid);
}

private void completeDepthBoneGpuJob(DepthBoneGpuQueuedJob job, Vec2Array offsets, bool valid) {
    depthBoneGpuCompletedJobs ~= DepthBoneGpuCompletedJob(
        job.packet, job.reason, job.actionSink, job.batchId, job.batchExpected, offsets, valid);
}

private bool depthBoneGpuPacketMatchesScope(
    ref DepthBoneGpuOffsetPacket packet,
    ExDepthRigRoot root,
    Parameter parameter,
    uint targetUuid
) {
    if (packet.root !is root) return false;
    if (parameter !is null && packet.parameter !is parameter) return false;
    auto targetNode = cast(Node)packet.target;
    return targetUuid == 0 || (targetNode !is null && targetNode.uuid == targetUuid);
}

private bool depthBoneGpuBatchHasPendingWork(uint batchId) {
    foreach (job; depthBoneGpuSubmissionQueue) if (job.batchId == batchId) return true;
    foreach (job; depthBoneGpuRefreshJobs) if (job.batchId == batchId) return true;
    foreach (job; depthBoneGpuCompletedJobs) if (job.batchId == batchId) return true;
    return false;
}

private void cleanupCanceledDepthBoneGpuBatch(uint batchId) {
    if (batchId !in canceledDepthBoneGpuBatches) return;
    if (!depthBoneGpuBatchHasPendingWork(batchId)) canceledDepthBoneGpuBatches.remove(batchId);
}

private void cancelDepthBoneGpuBatch(uint batchId) {
    canceledDepthBoneGpuBatches[batchId] = true;

    size_t i;
    while (i < depthBoneGpuSubmissionQueue.length) {
        if (depthBoneGpuSubmissionQueue[i].batchId == batchId) {
            depthBoneGpuSubmissionQueue =
                depthBoneGpuSubmissionQueue[0 .. i] ~ depthBoneGpuSubmissionQueue[i + 1 .. $];
            continue;
        }
        i++;
    }

    i = 0;
    while (i < depthBoneGpuCompletedJobs.length) {
        if (depthBoneGpuCompletedJobs[i].batchId == batchId) {
            depthBoneGpuCompletedJobs =
                depthBoneGpuCompletedJobs[0 .. i] ~ depthBoneGpuCompletedJobs[i + 1 .. $];
            continue;
        }
        i++;
    }
    cleanupCanceledDepthBoneGpuBatch(batchId);
}

private void cancelSupersededDepthBoneGpuWork(
    ExDepthRigRoot root,
    Parameter parameter,
    uint targetUuid,
    bool matchKeypoint = false,
    vec2u keypoint = vec2u.init,
) {
    uint[] batchIds;

    void include(uint batchId, ref DepthBoneGpuOffsetPacket packet) {
        if (!depthBoneGpuPacketMatchesScope(packet, root, parameter, targetUuid)) return;
        if (matchKeypoint && packet.keypoint != keypoint) return;
        foreach (existing; batchIds) if (existing == batchId) return;
        batchIds ~= batchId;
    }

    foreach (ref job; depthBoneGpuSubmissionQueue) include(job.batchId, job.packet);
    foreach (ref job; depthBoneGpuRefreshJobs) include(job.batchId, job.packet);
    foreach (ref job; depthBoneGpuCompletedJobs) include(job.batchId, job.packet);
    foreach (batchId; batchIds) cancelDepthBoneGpuBatch(batchId);
}

private void abortDepthBoneGpuRefresh() {
    depthBoneGpuSubmissionQueue = null;
    depthBoneGpuRefreshJobs = null;
    depthBoneGpuCompletedJobs = null;
    canceledDepthBoneGpuBatches = null;
}

private bool sameDepthBoneGpuWork(DepthBoneGpuOffsetPacket a, DepthBoneGpuOffsetPacket b) {
    return a.root is b.root &&
        a.target is b.target &&
        a.parameter is b.parameter &&
        a.keypoint == b.keypoint &&
        a.writePreview == b.writePreview &&
        a.writeBinding == b.writeBinding;
}

private bool hasOtherPendingDepthBoneGpuWork(DepthBoneGpuRefreshJob job) {
    foreach (queued; depthBoneGpuSubmissionQueue) {
        if (sameDepthBoneGpuWork(queued.packet, job.packet)) return true;
    }
    foreach (running; depthBoneGpuRefreshJobs) {
        if (running.jobId != job.jobId && sameDepthBoneGpuWork(running.packet, job.packet)) return true;
    }
    foreach (completed; depthBoneGpuCompletedJobs) {
        if (completed.valid && sameDepthBoneGpuWork(completed.packet, job.packet)) return true;
    }
    return false;
}

private void requeueStaleDepthBoneGpuJob(DepthBoneGpuRefreshJob job, string staleReason) {
    auto targetNode = cast(Node)job.packet.target;
    auto message = "Depth Bone GPU async job stale: target=%s key=(%s,%s) reason=%s".format(
        targetNode is null ? "(null)" : targetNode.name,
        job.packet.keypoint.x,
        job.packet.keypoint.y,
        staleReason);
    writeDepthBoneGpuFatalLog(message);
    if (job.packet.root is null ||
        !isLiveDepthRigRoot(job.packet.root) ||
        job.packet.parameter is null ||
        targetNode is null ||
        hasOtherPendingDepthBoneGpuWork(job)) return;
    if (job.packet.staleRetryCount >= DepthBoneGpuMaxStaleRetries) {
        writeDepthBoneGpuFatalLog(
            "Depth Bone GPU async job reached stale retry limit: target=%s key=(%s,%s) reason=%s".format(
                targetNode.name,
                job.packet.keypoint.x,
                job.packet.keypoint.y,
                staleReason));
        return;
    }

    auto bindingIndex = job.packet.root.findBindingIndex(targetNode.uuid);
    if (bindingIndex < 0) return;
    DepthBoneGpuOffsetPacket retryPacket;
    string buildError;
    if (!ngBuildDepthBoneGpuOffsetPacket(
        job.packet.root,
        &job.packet.root.bindings[cast(size_t)bindingIndex],
        job.packet.target,
        job.packet.parameter,
        job.packet.keypoint,
        retryPacket,
        buildError,
        job.packet.writePreview,
        job.packet.writeBinding,
    )) {
        writeDepthBoneGpuFatalLog(
            "Depth Bone GPU stale retry packet build failed: target=%s key=(%s,%s) reason=%s".format(
                targetNode.name,
                job.packet.keypoint.x,
                job.packet.keypoint.y,
                buildError));
        return;
    }
    retryPacket.staleRetryCount = job.packet.staleRetryCount + 1;
    enqueueDepthBoneGpuRefreshBatch([retryPacket], job.reason, job.actionSink);
    writeDepthBoneGpuFatalLog(
        "Depth Bone GPU async job requeued: target=%s key=(%s,%s) retry=%s/%s".format(
            targetNode.name,
            job.packet.keypoint.x,
            job.packet.keypoint.y,
            retryPacket.staleRetryCount,
            DepthBoneGpuMaxStaleRetries));
}

private bool sameVec2Array(Vec2Array a, Vec2Array b) {
    if (a.length != b.length) return false;
    foreach (i; 0 .. a.length) {
        if (abs(a[i].x - b[i].x) > 0.0001f || abs(a[i].y - b[i].y) > 0.0001f) return false;
    }
    return true;
}

private bool sameFloatArray(const(float)[] a, const(float)[] b) {
    if (a.length != b.length) return false;
    foreach (i; 0 .. a.length) {
        if (a[i] == b[i]) continue;
        auto bothNaN = a[i] != a[i] && b[i] != b[i];
        if (bothNaN) continue;
        if (!a[i].isFinite || !b[i].isFinite) return false;
        if (abs(a[i] - b[i]) > 0.0001f) return false;
    }
    return true;
}

private bool matrixApproxEqual(mat4 a, mat4 b) {
    foreach (x; 0 .. 4) {
        foreach (y; 0 .. 4) {
            if (abs(a[x][y] - b[x][y]) > 0.0001f) return false;
        }
    }
    return true;
}

private bool isDepthBoneGpuRefreshJobCurrent(ref DepthBoneGpuRefreshJob job, out string reason) {
    reason = null;
    auto target = job.packet.target;
    if (target is null) {
        reason = "target was deleted";
        return false;
    }
    if (!isLiveDepthRigRoot(job.packet.root)) {
        reason = "depth rig root is no longer live";
        return false;
    }
    if (!sameVec2Array(target.vertices, job.packet.vertices)) {
        reason = "target vertices changed while GPU job was pending";
        return false;
    }
    auto currentRawDepths = snapshotTargetDepths(target);
    if (!sameFloatArray(currentRawDepths, job.packet.rawDepths)) {
        reason = "target depths changed while GPU job was pending";
        return false;
    }
    auto targetNode = cast(Node)target;
    if (targetNode is null) {
        reason = "target was deleted";
        return false;
    }
    if (!job.packet.writeBinding) {
        auto currentTargetToRoot = targetToRootMatrix(job.packet.root, targetNode);
        if (!matrixApproxEqual(currentTargetToRoot, job.packet.targetToRoot)) {
            reason = "target transform changed while GPU job was pending";
            return false;
        }
    }
    auto bindingIndex = job.packet.root.findBindingIndex(target.uuid);
    if (bindingIndex < 0) {
        reason = "depth rig binding was removed while GPU job was pending";
        return false;
    }
    DepthBoneGpuOffsetPacket currentPacket;
    string buildError;
    if (!ngBuildDepthBoneGpuOffsetPacket(
        job.packet.root,
        &job.packet.root.bindings[cast(size_t)bindingIndex],
        target,
        job.packet.parameter,
        job.packet.keypoint,
        currentPacket,
        buildError,
        job.packet.writePreview,
        job.packet.writeBinding,
    )) {
        reason = "depth rig packet could not be rebuilt while GPU job was pending: " ~ buildError;
        return false;
    }
    if (currentPacket.boneCount != job.packet.boneCount ||
        currentPacket.sourceCount != job.packet.sourceCount ||
        currentPacket.maxInfluences != job.packet.maxInfluences ||
        abs(currentPacket.influenceRadiusFloor - job.packet.influenceRadiusFloor) > 0.0001f ||
        abs(currentPacket.radiusScale - job.packet.radiusScale) > 0.0001f ||
        !sameFloatArray(currentPacket.bones, job.packet.bones) ||
        !sameFloatArray(currentPacket.sourceInputs, job.packet.sourceInputs)) {
        reason = "depth rig binding changed while GPU job was pending";
        return false;
    }
    if (job.packet.parameter !is null) {
        if (depthBoneParameterStructureHash(job.packet.root, job.packet.parameter) != job.packet.parameterStructureHash) {
            reason = "depth bone parameter structure changed while GPU job was pending";
            return false;
        }
        if (depthBonePoseKeyHash(job.packet.root, job.packet.parameter, job.packet.keypoint) != job.packet.poseHash) {
            reason = "depth bone pose changed while GPU job was pending";
            return false;
        }
    }
    return true;
}

private bool isDepthBoneGpuQueuedJobCurrent(ref DepthBoneGpuQueuedJob job, out string reason) {
    DepthBoneGpuRefreshJob pending;
    pending.packet = job.packet;
    return isDepthBoneGpuRefreshJobCurrent(pending, reason);
}

private bool processDepthBoneGpuSubmissionQueue() {
    if (depthBoneGpuSubmissionQueue.length == 0) return false;
    bool submitted;
    size_t count;
    while (depthBoneGpuSubmissionQueue.length > 0 && count < DepthBoneGpuSubmissionsPerFrame) {
        auto queued = depthBoneGpuSubmissionQueue[0];
        if (queued.batchId in canceledDepthBoneGpuBatches) {
            depthBoneGpuSubmissionQueue = depthBoneGpuSubmissionQueue[1 .. $];
            cleanupCanceledDepthBoneGpuBatch(queued.batchId);
            continue;
        }

        string staleReason;
        if (!isDepthBoneGpuQueuedJobCurrent(queued, staleReason)) {
            auto queuedTarget = cast(Node)queued.packet.target;
            depthBoneDebugLog(
                "[DepthBoneRefresh] discard queued stale batch: batch=%s target=%s key=(%s,%s) reason=%s",
                queued.batchId,
                queuedTarget is null ? "(null)" : queuedTarget.name,
                queued.packet.keypoint.x,
                queued.packet.keypoint.y,
                staleReason);
            cancelDepthBoneGpuBatch(queued.batchId);
            continue;
        }
        if (depthBoneGpuRefreshJobs.length >= DepthBoneGpuMaxInFlight) break;

        depthBoneGpuSubmissionQueue = depthBoneGpuSubmissionQueue[1 .. $];
        string error;
        if (!submitDepthBoneGpuRefreshJob(queued, error)) {
            abortDepthBoneGpuRefresh();
            auto message = "Depth Bone GPU async submit failed: %s".format(error);
            writeDepthBoneGpuFatalLog(message);
            enforce(false, message);
        } else {
            submitted = true;
        }
        count++;
    }
    return submitted;
}

private size_t nodeHierarchyDepth(Node node) {
    size_t result;
    for (auto cursor = node; cursor !is null; cursor = cursor.parent) result++;
    return result;
}

private void appendUniqueGridAxis(ref float[] axis, float value) {
    foreach (existing; axis) {
        if (abs(existing - value) <= 0.0001f) return;
    }
    axis ~= value;
}

private ptrdiff_t gridVertexIndex(
    const(Vec2Array) vertices,
    float x,
    float y
) {
    foreach (i, vertex; vertices) {
        if (abs(vertex.x - x) <= 0.0001f &&
            abs(vertex.y - y) <= 0.0001f) return cast(ptrdiff_t)i;
    }
    return -1;
}

private size_t gridAxisInterval(const(float)[] axis, float value) {
    if (axis.length < 2) return 0;
    foreach (i; 0 .. axis.length - 1) {
        if (value <= axis[i + 1]) return i;
    }
    return axis.length - 2;
}

private bool prepareGridOffsetSampling(
    GridDeformer grid,
    Vec2Array offsets,
    out float[] axisX,
    out float[] axisY
) {
    if (grid is null || offsets.length != grid.vertices.length) return false;
    foreach (vertex; grid.vertices) {
        appendUniqueGridAxis(axisX, vertex.x);
        appendUniqueGridAxis(axisY, vertex.y);
    }
    axisX.sort();
    axisY.sort();
    return axisX.length >= 2 && axisY.length >= 2 &&
        axisX.length * axisY.length == grid.vertices.length;
}

private bool sampleGridOffset(
    GridDeformer grid,
    Vec2Array offsets,
    const(float)[] axisX,
    const(float)[] axisY,
    vec2 point,
    out vec2 sampled
) {
    sampled = vec2(0, 0);
    if (grid is null || offsets.length != grid.vertices.length ||
        axisX.length < 2 || axisY.length < 2) return false;

    auto x = min(max(point.x, axisX[0]), axisX[$ - 1]);
    auto y = min(max(point.y, axisY[0]), axisY[$ - 1]);
    auto xi = gridAxisInterval(axisX, x);
    auto yi = gridAxisInterval(axisY, y);
    auto xSpan = axisX[xi + 1] - axisX[xi];
    auto ySpan = axisY[yi + 1] - axisY[yi];
    auto u = xSpan > 0.0f ? (x - axisX[xi]) / xSpan : 0.0f;
    auto v = ySpan > 0.0f ? (y - axisY[yi]) / ySpan : 0.0f;

    auto i00 = gridVertexIndex(grid.vertices, axisX[xi], axisY[yi]);
    auto i10 = gridVertexIndex(grid.vertices, axisX[xi + 1], axisY[yi]);
    auto i01 = gridVertexIndex(grid.vertices, axisX[xi], axisY[yi + 1]);
    auto i11 = gridVertexIndex(grid.vertices, axisX[xi + 1], axisY[yi + 1]);
    if (i00 < 0 || i10 < 0 || i01 < 0 || i11 < 0) return false;

    sampled =
        offsets[cast(size_t)i00] * ((1.0f - u) * (1.0f - v)) +
        offsets[cast(size_t)i10] * (u * (1.0f - v)) +
        offsets[cast(size_t)i01] * ((1.0f - u) * v) +
        offsets[cast(size_t)i11] * (u * v);
    return true;
}

private bool addParentGridInfluence(
    GridDeformer parentGrid,
    Deformable target,
    Vec2Array parentOffsets,
    ref Vec2Array accumulated
) {
    if (parentGrid is null || target is null ||
        parentOffsets.length != parentGrid.vertices.length) return false;

    float[] axisX;
    float[] axisY;
    if (!prepareGridOffsetSampling(parentGrid, parentOffsets, axisX, axisY)) return false;

    if (accumulated.length != target.vertices.length) {
        accumulated.length = target.vertices.length;
        accumulated[] = vec2(0, 0);
    }

    auto targetToParent = parentGrid.transform.matrix.inverse * target.transform.matrix;
    auto parentToTarget = targetToParent.inverse;
    foreach (i, vertex; target.vertices) {
        auto parentPoint4 = targetToParent * vec4(vertex.x, vertex.y, 0.0f, 1.0f);
        auto parentPoint = vec2(parentPoint4.x, parentPoint4.y);
        if (parentGrid.dynamic && i < accumulated.length) {
            auto prior4 = targetToParent * vec4(
                accumulated[i].x, accumulated[i].y, 0.0f, 0.0f);
            parentPoint += vec2(prior4.x, prior4.y);
        }

        vec2 parentOffset;
        if (!sampleGridOffset(
            parentGrid, parentOffsets, axisX, axisY, parentPoint, parentOffset
        )) continue;
        auto targetOffset4 = parentToTarget * vec4(
            parentOffset.x, parentOffset.y, 0.0f, 0.0f);
        accumulated[i] += vec2(targetOffset4.x, targetOffset4.y);
    }
    return true;
}

private bool gridPropagationStopper(
    GridDeformer ancestor,
    Node target,
    out Node stopper
) {
    stopper = null;
    if (ancestor is null || target is null || ancestor is target) return false;
    Node[] intermediates;
    auto cursor = target.parent;
    while (cursor !is null && cursor !is ancestor) {
        intermediates ~= cursor;
        cursor = cursor.parent;
    }
    if (cursor !is ancestor) return false;
    foreach_reverse (intermediate; intermediates) {
        if (!intermediate.mustPropagate()) {
            stopper = intermediate;
            break;
        }
    }
    return true;
}

private bool depthBoneSourceIncludesAncestorPose(
    ExDepthBone targetSource,
    ExDepthBone ancestorSource
) {
    if (targetSource is null || ancestorSource is null) return false;
    auto cursor = targetSource;
    while (cursor !is null) {
        if (cursor is ancestorSource) return true;
        if (!cursor.allowParentToTargets || cursor.lockToRoot) return false;
        cursor = cast(ExDepthBone)cursor.parent;
    }
    return false;
}

private bool parentGridInfluenceCanReachTarget(
    ExDepthRigRoot root,
    GridDeformer parentGrid,
    Deformable target
) {
    if (root is null || parentGrid is null || target is null) return false;
    auto parentIndex = root.findBindingIndex(parentGrid.uuid);
    auto targetIndex = root.findBindingIndex((cast(Node)target).uuid);
    if (parentIndex < 0 || targetIndex < 0) return false;
    auto parentBinding = &root.bindings[cast(size_t)parentIndex];
    auto targetBinding = &root.bindings[cast(size_t)targetIndex];

    foreach (parentUuid; parentBinding.sourceBoneUuids) {
        auto parentSource = findBoneByUuid(root, parentUuid);
        if (parentSource is null) continue;
        foreach (targetUuid; targetBinding.sourceBoneUuids) {
            auto targetSource = findBoneByUuid(root, targetUuid);
            if (depthBoneSourceIncludesAncestorPose(targetSource, parentSource)) {
                return true;
            }
        }
    }
    return false;
}

private bool addParentGridNodeOriginInfluence(
    GridDeformer parentGrid,
    Node stopper,
    Deformable target,
    Vec2Array parentOffsets,
    ref Vec2Array accumulated
) {
    if (parentGrid is null || stopper is null || target is null ||
        cast(Deformable)stopper !is null || !parentGrid.translateChildren) return false;

    float[] axisX;
    float[] axisY;
    if (!prepareGridOffsetSampling(parentGrid, parentOffsets, axisX, axisY)) return false;

    auto stopperParentMatrix = stopper.parent is null
        ? mat4.identity
        : stopper.parent.transform.matrix;
    auto stopperToParentGrid = parentGrid.transform.matrix.inverse * stopperParentMatrix;
    auto origin4 = stopperToParentGrid * vec4(
        stopper.localTransform.translation.x,
        stopper.localTransform.translation.y,
        0.0f,
        1.0f
    );
    auto origin = vec2(origin4.x, origin4.y);
    if (parentGrid.dynamic) {
        auto offset4 = stopperToParentGrid * vec4(
            stopper.getValue("transform.t.x"),
            stopper.getValue("transform.t.y"),
            0.0f,
            0.0f
        );
        origin += vec2(offset4.x, offset4.y);
    }

    vec2 parentOffset;
    if (!sampleGridOffset(
        parentGrid, parentOffsets, axisX, axisY, origin, parentOffset
    )) return false;

    if (accumulated.length != target.vertices.length) {
        accumulated.length = target.vertices.length;
        accumulated[] = vec2(0, 0);
    }
    auto parentToTarget = target.transform.matrix.inverse * parentGrid.transform.matrix;
    auto targetOffset4 = parentToTarget * vec4(
        parentOffset.x, parentOffset.y, 0.0f, 0.0f);
    auto targetOffset = vec2(targetOffset4.x, targetOffset4.y);
    accumulated += targetOffset;
    return true;
}

private Vec2Array depthBoneAncestorGridInfluence(
    ExDepthRigRoot root,
    Deformable target,
    Parameter parameter,
    vec2u keypoint,
    Vec2Array targetOffsets,
    ref Vec2Array[uint] resolvedOffsets
) {
    Vec2Array result = targetOffsets.length == target.vertices.length
        ? targetOffsets.dup
        : Vec2Array.init;
    if (result.length != target.vertices.length) {
        result.length = target.vertices.length;
        result[] = vec2(0, 0);
    }
    auto original = result.dup;

    Node[] ancestors;
    for (auto cursor = (cast(Node)target).parent; cursor !is null; cursor = cursor.parent) {
        if (cast(GridDeformer)cursor) ancestors ~= cursor;
    }
    foreach_reverse (ancestorNode; ancestors) {
        auto parentGrid = cast(GridDeformer)ancestorNode;
        Node stopper;
        if (parentGrid is null ||
            root.findBindingIndex(parentGrid.uuid) < 0 ||
            !parentGridInfluenceCanReachTarget(root, parentGrid, target) ||
            !gridPropagationStopper(parentGrid, cast(Node)target, stopper)) continue;

        Vec2Array parentOffsets;
        if (auto resolved = cast(uint)parentGrid.uuid in resolvedOffsets) {
            parentOffsets = (*resolved).dup;
        } else if (parameter !is null) {
            auto binding = cast(DeformationParameterBinding)
                parameter.getBinding(parentGrid, "deform");
            if (binding !is null) {
                parentOffsets = binding.getValue(keypoint).vertexOffsets.dup;
            }
        } else {
            parentOffsets = parentGrid.deformation.dup;
        }
        if (stopper is null) {
            addParentGridInfluence(parentGrid, target, parentOffsets, result);
        } else {
            addParentGridNodeOriginInfluence(
                parentGrid, stopper, target, parentOffsets, result);
        }
    }
    result -= original;
    return result;
}

private bool applyDepthBoneGpuCompletedBatch(uint batchId) {
    DepthBoneGpuCompletedJob[] batch;
    foreach (job; depthBoneGpuCompletedJobs) {
        if (job.batchId == batchId) batch ~= job;
    }
    if (batch.length == 0 || batch.length < batch[0].batchExpected) return false;

    Parameter param;
    vec2u keypoint;
    string reason;
    GroupAction actionSink;
    DeformationParameterBinding[] deformBindings;
    Vec2Array[] offsetsList;
    ParameterBinding[] created;
    bool changed;
    size_t expectedBindingWrites;

    foreach (job; batch) {
        if (!job.valid) {
            size_t i;
            while (i < depthBoneGpuCompletedJobs.length) {
                if (depthBoneGpuCompletedJobs[i].batchId == batchId) {
                    depthBoneGpuCompletedJobs = depthBoneGpuCompletedJobs[0 .. i] ~ depthBoneGpuCompletedJobs[i + 1 .. $];
                    continue;
                }
                i++;
            }
            return false;
        }
    }

    auto hierarchyBatch = batch.dup;
    hierarchyBatch.sort!((a, b) =>
        nodeHierarchyDepth(cast(Node)a.packet.target) <
        nodeHierarchyDepth(cast(Node)b.packet.target));
    Vec2Array[uint] resolvedOffsets;
    foreach (job; hierarchyBatch) {
        auto target = job.packet.target;
        auto targetNode = cast(Node)target;
        if (target is null || targetNode is null) continue;
        auto adjusted = job.offsets.dup;
        foreach (_; 0 .. 6) {
            auto inherited = depthBoneAncestorGridInfluence(
                job.packet.root,
                target,
                job.packet.parameter,
                job.packet.keypoint,
                adjusted,
                resolvedOffsets
            );
            if (inherited.length != adjusted.length) break;
            auto next = job.offsets.dup;
            next -= inherited;
            auto converged = sameVec2Array(next, adjusted);
            adjusted = next;
            if (converged) break;
        }
        resolvedOffsets[targetNode.uuid] = adjusted;
    }

    foreach (job; batch) {
        auto target = job.packet.target;
        auto targetNode = cast(Node)target;
        if (target is null || targetNode is null) {
            abortDepthBoneGpuRefresh();
            enforce(false, "Depth Bone GPU writeback failed: target was deleted before batch writeback");
        }
        if (job.offsets.length != target.vertices.length) {
            abortDepthBoneGpuRefresh();
            enforce(false, "Depth Bone GPU writeback failed: target=%s key=(%s,%s) readback offsets=%s vertices=%s".format(
                targetNode.name,
                job.packet.keypoint.x,
                job.packet.keypoint.y,
                job.offsets.length,
                target.vertices.length));
        }

        auto adjusted = targetNode.uuid in resolvedOffsets;
        auto offsets = adjusted is null ? job.offsets : *adjusted;

        if (job.packet.writePreview) {
            target.deformation = offsets;
            target.notifyChange(target, NotifyReason.AttributeChanged);
            changed = true;
        }

        if (!job.packet.writeBinding || job.packet.parameter is null) continue;
        expectedBindingWrites++;

        param = job.packet.parameter;
        keypoint = job.packet.keypoint;
        reason = job.reason;
        actionSink = job.actionSink;
        auto existing = param.getBinding(targetNode, "deform");
        auto deformBinding = cast(DeformationParameterBinding)existing;
        if (deformBinding is null) {
            deformBinding = cast(DeformationParameterBinding)param.getOrAddBinding(targetNode, "deform");
            if (deformBinding !is null) created ~= deformBinding;
        }
        if (deformBinding is null) {
            abortDepthBoneGpuRefresh();
            enforce(false, "Depth Bone GPU writeback failed: target=%s key=(%s,%s) deform binding could not be created".format(
                targetNode.name,
                job.packet.keypoint.x,
                job.packet.keypoint.y));
        }
        deformBindings ~= deformBinding;
        offsetsList ~= offsets;
    }

    if (expectedBindingWrites != deformBindings.length) {
        abortDepthBoneGpuRefresh();
        enforce(false, "Depth Bone GPU writeback failed: batch=%s expected binding writes=%s actual=%s".format(
            batchId, expectedBindingWrites, deformBindings.length));
    }

    if (param !is null && deformBindings.length > 0) {
        auto group = new GroupAction();
        foreach (binding; created)
            group.addAction(new ParameterBindingAddAction(param, binding, false));
        auto label = reason.length ? _("Auto Refresh Depth Bone Deform: %s").format(reason) : _("Auto Refresh Depth Bone Deform");
        // Generated output is tagged at the action itself so its initial write,
        // undo, and redo cannot recursively invalidate the generator.
        auto action = new ParameterChangeBindingsValueAction(label, param, cast(ParameterBinding[])deformBindings,
            cast(int)keypoint.x, cast(int)keypoint.y, false);
        foreach (i, binding; deformBindings) binding.update(keypoint, offsetsList[i]);
        action.updateNewState();
        group.addAction(action);
        runWithDepthBoneRefreshActionSink(actionSink, {
            pushDepthBoneRefreshAction(group);
            return true;
        });
        changed = true;
    }

    size_t i;
    while (i < depthBoneGpuCompletedJobs.length) {
        if (depthBoneGpuCompletedJobs[i].batchId == batchId) {
            depthBoneGpuCompletedJobs = depthBoneGpuCompletedJobs[0 .. i] ~ depthBoneGpuCompletedJobs[i + 1 .. $];
            continue;
        }
        i++;
    }
    return changed;
}

private bool processDepthBoneGpuCompletedJobs() {
    bool changed;
    uint[] batchIds;
    foreach (job; depthBoneGpuCompletedJobs) {
        bool found;
        foreach (batchId; batchIds) {
            if (batchId == job.batchId) {
                found = true;
                break;
            }
        }
        if (!found) batchIds ~= job.batchId;
    }
    size_t processed;
    foreach (batchId; batchIds) {
        if (processed >= DepthBoneGpuCompletedBatchesPerFrame) break;
        changed = applyDepthBoneGpuCompletedBatch(batchId) || changed;
        processed++;
    }
    return changed;
}

private bool processDepthBoneGpuRefreshJobs() {
    if (depthBoneGpuRefreshJobs.length == 0) return false;
    bool changed;
    size_t processed;
    size_t i;
    while (i < depthBoneGpuRefreshJobs.length && processed < DepthBoneGpuReadbacksPerFrame) {
        auto job = depthBoneGpuRefreshJobs[i];
        NgDepthBoneGpuAsyncResult result;
        string error;
        if (!ngPollDepthBoneGpuAsync(job.jobId, result, error)) {
            abortDepthBoneGpuRefresh();
            auto message = "Depth Bone GPU async poll failed: %s".format(error);
            writeDepthBoneGpuFatalLog(message);
            enforce(false, message);
        }
        if (!result.ready) {
            i++;
            continue;
        }
        if (job.batchId in canceledDepthBoneGpuBatches) {
            depthBoneGpuRefreshJobs =
                depthBoneGpuRefreshJobs[0 .. i] ~ depthBoneGpuRefreshJobs[i + 1 .. $];
            cleanupCanceledDepthBoneGpuBatch(job.batchId);
            processed++;
            continue;
        }
        string staleReason;
        if (!isDepthBoneGpuRefreshJobCurrent(job, staleReason)) {
            requeueStaleDepthBoneGpuJob(job, staleReason);
            completeDepthBoneGpuJob(job, Vec2Array.init, false);
            depthBoneGpuRefreshJobs = depthBoneGpuRefreshJobs[0 .. i] ~ depthBoneGpuRefreshJobs[i + 1 .. $];
            processed++;
            continue;
        }
        auto offsets = ngDepthBoneGpuReadbackToOffsets(result.xs, result.ys);
        completeDepthBoneGpuJob(job, offsets, true);
        depthBoneGpuRefreshJobs = depthBoneGpuRefreshJobs[0 .. i] ~ depthBoneGpuRefreshJobs[i + 1 .. $];
        processed++;
    }
    return processDepthBoneGpuCompletedJobs() || changed;
}

private string dirtyScopeName(DepthBoneDirtyScope dirtyScope) {
    return dirtyScope == DepthBoneDirtyScope.AllKeypoints ? "all-keypoints" : "keypoint";
}

private bool sameDirtyParameter(DepthBoneDirtyRequest request, ExDepthRigRoot root, Parameter parameter, uint targetUuid) {
    return request.root is root &&
        request.parameter is parameter &&
        request.targetUuid == targetUuid &&
        request.actionSink is depthBoneRefreshActionSink;
}

private bool depthBoneParameterDrivesRig(ExDepthRigRoot root, Parameter param) {
    if (root is null || param is null) return false;
    foreach (binding; param.bindings) {
        auto bone = cast(ExDepthBone)binding.getTarget().node;
        if (bone is null || !rootContainsBone(root, bone)) continue;
        if (isDepthBoneTransformBindingName(binding.getName())) return true;
    }
    return false;
}

private Parameter depthBoneRefreshParameter(ExDepthRigRoot root, Parameter candidate) {
    return depthBoneParameterDrivesRig(root, candidate) ? candidate : null;
}

void ngMarkDepthBoneDirty(
    ExDepthRigRoot root,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope.Keypoint,
    uint targetUuid = 0,
    bool settleBeforeDispatch = false,
) {
    if (root is null) return;
    ngInvalidateDepthBoneEffectivePivotCache(root);
    if (parameter !is null) {
        lastDepthBoneDirtyRoot = root;
        lastDepthBoneDirtyParameter = parameter;
        lastDepthBoneDirtyKeypoint = keypoint;
    }
    depthBoneDebugLog("[DepthBoneRefresh] mark: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
        root.name,
        parameter is null ? "(none)" : parameter.name,
        keypoint.x,
        keypoint.y,
        dirtyScopeName(dirtyScope),
        reason);
    foreach (ref request; depthBoneDirtyRequests) {
        if (!sameDirtyParameter(request, root, parameter, targetUuid)) continue;
        if (request.dirtyScope == DepthBoneDirtyScope.AllKeypoints || dirtyScope == DepthBoneDirtyScope.AllKeypoints) {
            request.dirtyScope = DepthBoneDirtyScope.AllKeypoints;
            request.keypoint = keypoint;
            request.settleBeforeDispatch =
                request.settleBeforeDispatch || settleBeforeDispatch;
            if (reason.length > 0) request.reason = reason;
            depthBoneDebugLog("[DepthBoneRefresh] mark merged: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
                root.name,
                parameter is null ? "(none)" : parameter.name,
                keypoint.x,
                keypoint.y,
                dirtyScopeName(request.dirtyScope),
                request.reason);
            return;
        }
        if (request.keypoint == keypoint) {
            if (reason.length > 0) request.reason = reason;
            depthBoneDebugLog("[DepthBoneRefresh] mark merged: root=%s param=%s key=(%s,%s) scope=%s reason=%s",
                root.name,
                parameter is null ? "(none)" : parameter.name,
                keypoint.x,
                keypoint.y,
                dirtyScopeName(request.dirtyScope),
                request.reason);
            return;
        }
    }
    depthBoneDirtyRequests ~= DepthBoneDirtyRequest(
        root,
        parameter,
        keypoint,
        targetUuid,
        dirtyScope,
        reason,
        depthBoneRefreshActionSink,
        settleBeforeDispatch);
}

void ngMarkDepthBoneDirtyForArmedParameter(
    ExDepthRigRoot root,
    string reason,
    DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope.Keypoint,
) {
    auto param = depthBoneRefreshParameter(root, incArmedParameter());
    auto keypoint = param is null ? vec2u.init : param.findClosestKeypoint();
    if (param is null && lastDepthBoneDirtyRoot is root) {
        param = depthBoneRefreshParameter(root, lastDepthBoneDirtyParameter);
    }
    if (param !is null && lastDepthBoneDirtyRoot is root && param is lastDepthBoneDirtyParameter) {
        keypoint = lastDepthBoneDirtyKeypoint;
    }
    ngMarkDepthBoneDirty(root, param, keypoint, reason, dirtyScope);
}

void ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(ExDepthRigRoot root, string reason) {
    ngMarkDepthBoneDirtyForArmedParameter(root, reason, DepthBoneDirtyScope.AllKeypoints);
}

void ngMarkDepthBoneDirtyForTarget(Node target, string reason) {
    if (target is null || incActivePuppet() is null) return;

    if (auto bone = cast(ExDepthBone)target) {
        if (auto root = findDepthRigRoot(bone)) {
            if (!depthBoneRootHasGeneratedTargets(root)) return;
            if (depthBoneAffectedParameters(root).length == 0) return;
            ngMarkDepthBoneDirty(
                root, null, vec2u.init, reason, DepthBoneDirtyScope.AllKeypoints);
        }
        return;
    }

    if (auto root = cast(ExDepthRigRoot)target) {
        if (!depthBoneRootHasGeneratedTargets(root)) return;
        if (depthBoneAffectedParameters(root).length == 0) return;
        ngMarkDepthBoneDirty(
            root, null, vec2u.init, reason, DepthBoneDirtyScope.AllKeypoints);
        return;
    }

    foreach (root; depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool exactTarget;
        bool affectsDescendant;
        foreach (ref binding; root.bindings) {
            auto bindingTarget = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
            if (bindingTarget is null) continue;
            if (bindingTarget is target) exactTarget = true;
            else if (isSameOrAncestorNode(target, bindingTarget)) affectsDescendant = true;
        }
        if (!exactTarget && !affectsDescendant) continue;
        if (depthBoneAffectedParameters(root).length == 0) continue;

        // Descendants share hierarchy compensation and must refresh together.
        auto affectedTargetUuid = affectsDescendant ? 0 : cast(uint)target.uuid;
        ngMarkDepthBoneDirty(
            root,
            null,
            vec2u.init,
            reason,
            DepthBoneDirtyScope.AllKeypoints,
            affectedTargetUuid);
    }
}

private bool isSameOrAncestorNode(Node ancestor, Node node) {
    auto cursor = node;
    while (cursor !is null) {
        if (cursor is ancestor) return true;
        cursor = cursor.parent;
    }
    return false;
}

private bool ngMarkDepthBoneDirtyForTransformBindingTarget(Node changedNode, Parameter param, vec2u kp, string reason) {
    if (changedNode is null || param is null || incActivePuppet() is null) return false;
    bool marked;
    foreach (root; depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool affectsRoot;
        foreach (ref binding; root.bindings) {
            auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
            if (targetNode is null) continue;
            if (changedNode is targetNode || isSameOrAncestorNode(changedNode, targetNode) || changedNode is root) {
                affectsRoot = true;
                break;
            }
        }
        if (!affectsRoot) continue;
        ngMarkDepthBoneDirty(root, param, kp, reason, DepthBoneDirtyScope.Keypoint);
        marked = true;
    }
    return marked;
}

private bool depthBoneBindingMutationInfo(
    ParameterBinding binding,
    out Parameter parameter,
    out Node target,
    out string bindingName,
) {
    parameter = null;
    target = null;
    bindingName = null;
    if (auto valueBinding = cast(ValueParameterBinding)binding) {
        parameter = valueBinding.parameter;
        target = cast(Node)valueBinding.getTarget().target;
        bindingName = valueBinding.getName();
        return parameter !is null && target !is null;
    }
    if (auto deformBinding = cast(DeformationParameterBinding)binding) {
        parameter = deformBinding.parameter;
        target = cast(Node)deformBinding.getTarget().target;
        bindingName = deformBinding.getName();
        return parameter !is null && target !is null;
    }
    return false;
}

private bool ngMarkDepthBoneDirtyForDeformationBindingTarget(
    Node changedNode,
    Parameter param,
    vec2u kp,
    DepthBoneDirtyScope dirtyScope,
    string reason,
) {
    auto parentGrid = cast(GridDeformer)changedNode;
    if (parentGrid is null || param is null || incActivePuppet() is null) return false;

    bool marked;
    foreach (root; depthBoneRoots()) {
        if (!depthBoneRootHasGeneratedTargets(root)) continue;
        bool affectsDescendant;
        foreach (ref rigBinding; root.bindings) {
            auto targetNode = incActivePuppet().find!Node(cast(uint)rigBinding.targetUuid);
            auto target = cast(Deformable)targetNode;
            if (targetNode is null || target is null || targetNode is changedNode) continue;
            if (!isSameOrAncestorNode(changedNode, targetNode)) continue;
            if (!parentGridInfluenceCanReachTarget(root, parentGrid, target)) continue;
            affectsDescendant = true;
            break;
        }
        if (!affectsDescendant) continue;

        // Parent and descendant targets must be generated in one hierarchy batch;
        // splitting by target would make descendant compensation read stale data.
        ngMarkDepthBoneDirty(root, param, kp, reason, dirtyScope);
        marked = true;
    }
    return marked;
}

void ngDepthBoneMutationChanged(DepthBoneMutation mutation) {
    final switch (mutation.kind) {
        case DepthBoneMutationKind.BindingValue:
        case DepthBoneMutationKind.BindingAllValues:
        case DepthBoneMutationKind.BindingStructure:
            Parameter param;
            Node target;
            string bindingName;
            if (!depthBoneBindingMutationInfo(
                mutation.binding, param, target, bindingName)) return;

            auto dirtyScope = mutation.kind == DepthBoneMutationKind.BindingValue
                ? DepthBoneDirtyScope.Keypoint
                : DepthBoneDirtyScope.AllKeypoints;
            if (cast(DeformationParameterBinding)mutation.binding) {
                ngMarkDepthBoneDirtyForDeformationBindingTarget(
                    target,
                    param,
                    mutation.keypoint,
                    dirtyScope,
                    "Depth Bone Ancestor Deformation");
                return;
            }
            if (!isDepthBoneTransformBindingName(bindingName)) return;
            if (auto bone = cast(ExDepthBone)target) {
                auto root = findDepthRigRoot(bone);
                if (root !is null && depthBoneRootHasGeneratedTargets(root)) {
                    ngMarkDepthBoneDirty(
                        root,
                        param,
                        mutation.keypoint,
                        "Depth Bone Transform",
                        dirtyScope);
                }
                return;
            }
            if (dirtyScope == DepthBoneDirtyScope.Keypoint) {
                ngMarkDepthBoneDirtyForTransformBindingTarget(
                    target, param, mutation.keypoint, "Depth Bone Target Transform");
            } else {
                foreach (root; depthBoneRoots()) {
                    if (!depthBoneRootHasGeneratedTargets(root)) continue;
                    bool affectsRoot = target is root;
                    foreach (ref rigBinding; root.bindings) {
                        auto targetNode = incActivePuppet().find!Node(cast(uint)rigBinding.targetUuid);
                        if (targetNode !is null &&
                            (target is targetNode || isSameOrAncestorNode(target, targetNode))) {
                            affectsRoot = true;
                            break;
                        }
                    }
                    if (affectsRoot) {
                        ngMarkDepthBoneDirty(
                            root,
                            param,
                            mutation.keypoint,
                            "Depth Bone Target Transform",
                            DepthBoneDirtyScope.AllKeypoints);
                    }
                }
            }
            return;

        case DepthBoneMutationKind.RigConfiguration:
            ExDepthRigRoot root;
            if (auto directRoot = cast(ExDepthRigRoot)mutation.target) {
                root = directRoot;
            } else if (auto bone = cast(ExDepthBone)mutation.target) {
                root = findDepthRigRoot(bone);
            }
            if (root !is null) {
                if (!depthBoneRootHasGeneratedTargets(root)) return;
                if (depthBoneAffectedParameters(root).length == 0) return;
                ngMarkDepthBoneDirty(
                    root,
                    null,
                    vec2u.init,
                    mutation.reason,
                    DepthBoneDirtyScope.AllKeypoints,
                    0,
                    mutation.settleBeforeDispatch);
            }
            return;

        case DepthBoneMutationKind.TargetTransform:
        case DepthBoneMutationKind.TargetGeometry:
            ngMarkDepthBoneDirtyForTarget(mutation.target, mutation.reason);
            return;
    }
}

private ulong hashMix(ulong seed, ulong value) {
    seed ^= value + 0x9e3779b97f4a7c15UL + (seed << 6) + (seed >> 2);
    return seed;
}

private ulong hashBool(ulong seed, bool value) {
    return hashMix(seed, value ? 1UL : 0UL);
}

private ulong hashInt(ulong seed, long value) {
    return hashMix(seed, cast(ulong)value);
}

private ulong hashFloat(ulong seed, float value) {
    if (!value.isFinite) return hashMix(seed, 0x7ff80000UL);
    return hashInt(seed, cast(long)(value * 1000000.0f));
}

private ulong hashVec2(ulong seed, vec2 value) {
    seed = hashFloat(seed, value.x);
    return hashFloat(seed, value.y);
}

private ulong hashVec3(ulong seed, vec3 value) {
    seed = hashFloat(seed, value.x);
    seed = hashFloat(seed, value.y);
    return hashFloat(seed, value.z);
}

private ulong hashMatrix(ulong seed, mat4 matrix) {
    foreach (x; 0 .. 4) {
        foreach (y; 0 .. 4) {
            seed = hashFloat(seed, matrix[x][y]);
        }
    }
    return seed;
}

private ulong hashStringValue(ulong seed, string value) {
    foreach (ch; value) seed = hashMix(seed, cast(ulong)ch);
    return seed;
}

private bool isDepthBoneTransformBindingName(string bindingName) {
    return bindingName.startsWith("transform.t.")
        || bindingName.startsWith("transform.r.")
        || bindingName.startsWith("transform.s.");
}

private ulong hashDepthBoneTargetStructure(ulong hash, Node targetNode) {
    if (targetNode is null) return hashBool(hash, false);
    hash = hashBool(hash, true);
    hash = hashStringValue(hash, targetNode.typeId);

    if (auto grid = cast(GridDeformer)targetNode) {
        hash = hashStringValue(hash, "GridDeformer");
        size_t cols;
        size_t rows;
        float[] xs;
        float[] ys;
        foreach (vertex; grid.vertices) {
            bool hasX;
            foreach (x; xs) {
                if (abs(x - vertex.x) <= 0.0001f) {
                    hasX = true;
                    break;
                }
            }
            if (!hasX) xs ~= vertex.x;
            bool hasY;
            foreach (y; ys) {
                if (abs(y - vertex.y) <= 0.0001f) {
                    hasY = true;
                    break;
                }
            }
            if (!hasY) ys ~= vertex.y;
        }
        cols = xs.length;
        rows = ys.length;
        hash = hashInt(hash, cast(long)cols);
        hash = hashInt(hash, cast(long)rows);
        hash = hashInt(hash, cast(long)grid.gridFormation);
        hash = hashBool(hash, grid.dynamic);
        hash = hashBool(hash, grid.translateChildren);
        return hash;
    }

    if (auto path = cast(PathDeformer)targetNode) {
        hash = hashStringValue(hash, "PathDeformer");
        hash = hashInt(hash, cast(long)path.curveType);
        hash = hashInt(hash, cast(long)path.physicsType);
        hash = hashBool(hash, path.dynamic);
        hash = hashBool(hash, path.physicsOnly);
        hash = hashBool(hash, path.physicsEnabled);
        return hash;
    }

    return hash;
}

private ExDepthRigRoot[] depthBoneRoots() {
    ExDepthRigRoot[] result;
    auto puppet = incActivePuppet();
    if (puppet is null || puppet.root is null) return result;

    void visit(Node node) {
        if (node is null) return;
        if (auto root = cast(ExDepthRigRoot)node) result ~= root;
        foreach (child; node.children) visit(child);
    }

    visit(puppet.root);
    return result;
}

private bool isLiveDepthRigRoot(ExDepthRigRoot root) {
    if (root is null) return false;
    foreach (liveRoot; depthBoneRoots()) {
        if (liveRoot is root) return true;
    }
    return false;
}

private bool depthBoneRootHasGeneratedTargets(ExDepthRigRoot root) {
    auto puppet = incActivePuppet();
    if (root is null || puppet is null) return false;
    foreach (ref binding; root.bindings) {
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto target = puppet.find!Node(cast(uint)binding.targetUuid);
        if (cast(Deformable)target !is null) return true;
    }
    return false;
}

private bool rootContainsBone(ExDepthRigRoot root, ExDepthBone bone) {
    if (root is null || bone is null) return false;
    Node cursor = bone;
    while (cursor !is null) {
        if (cursor is root) return true;
        cursor = cursor.parent;
    }
    return false;
}

private string keypointKey(vec2u kp) {
    return kp.x.to!string ~ ":" ~ kp.y.to!string;
}

private ulong depthBoneRigStructureHash(ExDepthRigRoot root) {
    auto puppet = incActivePuppet();
    ulong hash = 1469598103934665603UL;
    if (root is null || puppet is null) return hash;

    hash = hashInt(hash, cast(long)root.uuid);
    hash = hashMatrix(hash, root.transform.matrix);

    foreach (bone; root.depthBones()) {
        hash = hashInt(hash, cast(long)bone.uuid);
        hash = hashStringValue(hash, bone.boneId);
        hash = hashVec3(hash, bone.restHead);
        hash = hashVec3(hash, bone.restTail);
        hash = hashFloat(hash, bone.restRoll);
        hash = hashBool(hash, bone.lockRotation);
        hash = hashBool(hash, bone.lockTranslation);
        hash = hashBool(hash, bone.lockToRoot);
        hash = hashBool(hash, bone.allowParentToTargets);
        hash = hashVec3(hash, bone.localTransform.translation);
        hash = hashVec3(hash, bone.localTransform.rotation);
        hash = hashVec2(hash, bone.localTransform.scale);
        hash = hashInt(hash, bone.parent is null ? 0 : cast(long)bone.parent.uuid);
    }

    foreach (ref binding; root.bindings) {
        hash = hashInt(hash, cast(long)binding.targetUuid);
        hash = hashInt(hash, cast(long)binding.targetKind);
        hash = hashInt(hash, cast(long)binding.influenceRule.maxInfluences);
        hash = hashFloat(hash, binding.influenceRule.radiusScale);
        hash = hashFloat(hash, binding.influenceRule.minimumRadius);
        hash = hashStringValue(hash, binding.influenceRule.falloff);
        foreach (uuid; binding.sourceBoneUuids) {
            hash = hashInt(hash, cast(long)uuid);
            auto setting = binding.sourceSetting(uuid);
            hash = hashFloat(hash, setting.weight);
            hash = hashFloat(hash, setting.depthOffset);
            hash = hashFloat(hash, setting.depthScale);
            hash = hashFloat(hash, normalizeDepthBoneSourceRotation(setting.rotation));
            if (auto multiplier = uuid in binding.influenceRule.multipliersByBoneUuid) hash = hashFloat(hash, *multiplier);
        }

        auto targetNode = puppet.find!Node(cast(uint)binding.targetUuid);
        hash = hashBool(hash, targetNode !is null);
        if (targetNode !is null) {
            hash = hashBool(hash, targetNode.lockToRoot);
            hash = hashMatrix(hash, targetToRootMatrix(root, targetNode));
            hash = hashDepthBoneTargetStructure(hash, targetNode);
            if (auto deformable = cast(Deformable)targetNode) {
                hash = hashInt(hash, cast(long)deformable.vertices.length);
                foreach (vertex; deformable.vertices) hash = hashVec2(hash, vertex);
            }
        }
    }

    return hash;
}

private ulong depthBoneParameterStructureHash(ExDepthRigRoot root, Parameter param) {
    ulong hash = 1099511628211UL;
    if (root is null || param is null) return hash;

    hash = hashInt(hash, cast(long)param.uuid);
    hash = hashBool(hash, param.isVec2);
    hash = hashVec2(hash, param.min);
    hash = hashVec2(hash, param.max);
    foreach (axis; 0 .. 2) {
        hash = hashInt(hash, cast(long)param.axisPoints[axis].length);
        foreach (point; param.axisPoints[axis]) hash = hashFloat(hash, point);
    }

    foreach (binding; param.bindings) {
        auto bone = cast(ExDepthBone)binding.getTarget().node;
        if (bone is null || !rootContainsBone(root, bone)) continue;
        if (!isDepthBoneTransformBindingName(binding.getName())) continue;
        hash = hashInt(hash, cast(long)bone.uuid);
        hash = hashStringValue(hash, binding.getName());
    }

    return hash;
}

private ulong depthBonePoseKeyHash(ExDepthRigRoot root, Parameter param, vec2u kp) {
    ulong hash = 7809847782465536322UL;
    if (root is null || param is null) return hash;

    foreach (binding; param.bindings) {
        auto bone = cast(ExDepthBone)binding.getTarget().node;
        if (bone is null || !rootContainsBone(root, bone)) continue;
        if (!isDepthBoneTransformBindingName(binding.getName())) continue;
        auto valueBinding = cast(ValueParameterBinding)binding;
        if (valueBinding is null) continue;
        hash = hashInt(hash, cast(long)bone.uuid);
        hash = hashStringValue(hash, valueBinding.getName());
        bool isSet = valueBinding.isSet_[kp.x][kp.y];
        hash = hashBool(hash, isSet);
        if (isSet) hash = hashFloat(hash, valueBinding.values[kp.x][kp.y]);
    }

    return hash;
}

private vec2u[] depthBoneKeypoints(Parameter param) {
    vec2u[] result;
    if (param is null) return result;
    foreach (x; 0 .. param.axisPointCount(0)) {
        foreach (y; 0 .. param.axisPointCount(1)) {
            result ~= vec2u(cast(uint)x, cast(uint)y);
        }
    }
    if (result.length == 0) result ~= param.findClosestKeypoint();
    return result;
}

private Parameter[] depthBoneAffectedParameters(ExDepthRigRoot rigRoot) {
    Parameter[] result;
    auto puppet = incActivePuppet();
    if (rigRoot is null || puppet is null) return result;

    bool hasParam(Parameter param) {
        foreach (existing; result) if (existing is param) return true;
        return false;
    }

    auto armed = incArmedParameter();
    if (depthBoneParameterDrivesRig(rigRoot, armed)) result ~= armed;
    if (lastDepthBoneDirtyRoot is rigRoot &&
        depthBoneParameterDrivesRig(rigRoot, lastDepthBoneDirtyParameter) &&
        !hasParam(lastDepthBoneDirtyParameter)) {
        result ~= lastDepthBoneDirtyParameter;
    }

    foreach (param; puppet.parameters) {
        if (param is null || hasParam(param)) continue;
        if (depthBoneParameterDrivesRig(rigRoot, param)) result ~= param;
    }
    return result;
}

private bool depthBoneBindingMatchesTarget(ref ExDepthRigBinding binding, uint targetUuid) {
    return targetUuid == 0 || binding.targetUuid == targetUuid;
}

private bool ngRefreshDepthBoneDeform(ExDepthRigRoot rigRoot, Parameter param, vec2u kp, string reason, uint targetUuid = 0) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (rigRoot is null || incActivePuppet() is null) return false;
    depthBoneDebugLog("[DepthBoneRefresh] refresh start: root=%s param=%s key=(%s,%s) reason=%s bindings=%s",
        rigRoot.name,
        param is null ? "(none)" : param.name,
        kp.x,
        kp.y,
        reason,
        rigRoot.bindings.length);

    enforceDepthBoneGpuAvailable("Depth Bone dirty refresh");
    DepthBoneGpuOffsetPacket[] packets;
    foreach (ref binding; rigRoot.bindings) {
        if (!depthBoneBindingMatchesTarget(binding, targetUuid)) continue;
        if (!hasValidDepthBoneSources(rigRoot, binding)) continue;
        auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
        auto deformable = cast(Deformable)targetNode;
        if (deformable is null) {
            auto message = "Depth Bone GPU packet build failed: target uuid=%s is missing or not deformable".format(
                binding.targetUuid);
            writeDepthBoneGpuFatalLog(message);
            enforce(false, message);
        }
        DepthBoneGpuOffsetPacket packet;
        string error;
        if (!ngBuildDepthBoneGpuOffsetPacket(rigRoot, &binding, deformable, param, kp, packet, error, true, param !is null)) {
            auto message = "Depth Bone GPU packet build failed: target=%s key=(%s,%s) reason=%s".format(
                targetNode is null ? "(null)" : targetNode.name,
                kp.x,
                kp.y,
                error);
            writeDepthBoneGpuFatalLog(message);
            enforce(false, message);
        }
        packets ~= packet;
    }
    if (packets.length > 0) {
        cancelSupersededDepthBoneGpuWork(rigRoot, param, targetUuid, true, kp);
    }
    enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
    return packets.length > 0;
}

private bool ngRefreshDepthBoneDeformKeypoints(
    ExDepthRigRoot rigRoot,
    Parameter param,
    vec2u[] keypoints,
    vec2u visualKeypoint,
    string reason,
    uint targetUuid = 0,
) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (rigRoot is null || incActivePuppet() is null) return false;
    if (param is null || keypoints.length == 0) return false;
    depthBoneDebugLog("[DepthBoneRefresh] refresh start: root=%s param=%s scope=all-keypoints current=(%s,%s) reason=%s keypoints=%s bindings=%s",
        rigRoot.name,
        param.name,
        visualKeypoint.x,
        visualKeypoint.y,
        reason,
        keypoints.length,
        rigRoot.bindings.length);

    enforceDepthBoneGpuAvailable("Depth Bone all-keypoints refresh");
    bool submitted;
    foreach (kp; keypoints) {
        DepthBoneGpuOffsetPacket[] packets;
        foreach (ref binding; rigRoot.bindings) {
            if (!depthBoneBindingMatchesTarget(binding, targetUuid)) continue;
            if (!hasValidDepthBoneSources(rigRoot, binding)) continue;
            auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
            auto deformable = cast(Deformable)targetNode;
            if (deformable is null) {
                auto message = "Depth Bone GPU packet build failed: target uuid=%s is missing or not deformable".format(
                    binding.targetUuid);
                writeDepthBoneGpuFatalLog(message);
                enforce(false, message);
            }

            DepthBoneGpuOffsetPacket packet;
            string error;
            if (!ngBuildDepthBoneGpuOffsetPacket(
                rigRoot, &binding, deformable, param, kp, packet, error, kp == visualKeypoint, true
            )) {
                auto message = "Depth Bone GPU packet build failed: target=%s key=(%s,%s) reason=%s".format(
                    targetNode is null ? "(null)" : targetNode.name,
                    kp.x,
                    kp.y,
                    error);
                writeDepthBoneGpuFatalLog(message);
                enforce(false, message);
            }
            packets ~= packet;
        }
        enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
        submitted = packets.length > 0 || submitted;
    }
    return submitted;
}

private bool enqueueDepthBoneAllKeypoints(
    ExDepthRigRoot rigRoot,
    Parameter param,
    string reason,
    uint targetUuid = 0,
    bool settleBeforeDispatch = false,
) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (rigRoot is null || param is null) return false;
    auto keypoints = depthBoneKeypoints(param);
    if (keypoints.length == 0) return false;
    cancelSupersededDepthBoneGpuWork(rigRoot, param, targetUuid);

    foreach (ref job; depthBoneAllKeypointJobs) {
        if (job.root is rigRoot &&
            job.parameter is param &&
            job.targetUuid == targetUuid &&
            job.actionSink is depthBoneRefreshActionSink) {
            job.keypoints = keypoints;
            job.processed.clear();
            job.nextIndex = 0;
            job.reason = reason;
            job.settleFrames = settleBeforeDispatch ? 1 : 0;
            return true;
        }
    }

    DepthBoneAllKeypointJob job;
    job.root = rigRoot;
    job.parameter = param;
    job.targetUuid = targetUuid;
    job.keypoints = keypoints;
    job.reason = reason;
    job.actionSink = depthBoneRefreshActionSink;
    job.settleFrames = settleBeforeDispatch ? 1 : 0;
    depthBoneAllKeypointJobs ~= job;
    return true;
}

private bool ngRefreshDepthBoneDeformAllKeypoints(
    ExDepthRigRoot rigRoot,
    Parameter param,
    vec2u currentKeypoint,
    string reason,
    uint targetUuid = 0,
    bool settleBeforeDispatch = false,
) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (param is null) {
        auto params = depthBoneAffectedParameters(rigRoot);
        depthBoneDebugLog("[DepthBoneRefresh] resolve parameters: root=%s reason=%s resolved=%s",
            rigRoot is null ? "(null)" : rigRoot.name,
            reason,
            params.length);
        if (params.length == 0) return false;
        bool queued;
        foreach (resolvedParam; params) {
            queued = enqueueDepthBoneAllKeypoints(
                rigRoot,
                resolvedParam,
                reason,
                targetUuid,
                settleBeforeDispatch) || queued;
        }
        return queued;
    }
    return enqueueDepthBoneAllKeypoints(
        rigRoot,
        param,
        reason,
        targetUuid,
        settleBeforeDispatch);
}

private bool processDepthBoneAllKeypointJobs() {
    if (depthBoneAllKeypointJobs.length == 0) return false;

    size_t remaining = DepthBoneAllKeypointsPerFrame;
    bool changed;
    size_t i;
    while (i < depthBoneAllKeypointJobs.length) {
        auto job = &depthBoneAllKeypointJobs[i];
        if (job.root is null || !isLiveDepthRigRoot(job.root) || job.parameter is null || incActivePuppet() is null) {
            depthBoneAllKeypointJobs = depthBoneAllKeypointJobs[0 .. i] ~ depthBoneAllKeypointJobs[i + 1 .. $];
            continue;
        }
        if (job.settleFrames > 0) {
            job.settleFrames--;
            i++;
            continue;
        }

        vec2u[] chunk;
        auto visual = job.parameter.findClosestKeypoint();
        size_t neededCount = job.parameter.isVec2 ? 4 : 2;
        foreach (kp; job.keypoints) {
            if (neededCount == 0) break;
            if (kp.x < visual.x || kp.x > visual.x + 1 || kp.y < visual.y || kp.y > visual.y + 1) continue;
            auto key = keypointKey(kp);
            if (key in job.processed) continue;
            chunk ~= kp;
            job.processed[key] = true;
            neededCount--;
        }

        while (remaining > 0 && job.nextIndex < job.keypoints.length) {
            auto kp = job.keypoints[job.nextIndex++];
            auto key = keypointKey(kp);
            if (key in job.processed) continue;
            chunk ~= kp;
            job.processed[key] = true;
            remaining--;
        }

        if (chunk.length > 0) {
            changed = runWithDepthBoneRefreshActionSink(job.actionSink, {
                return ngRefreshDepthBoneDeformKeypoints(
                    job.root, job.parameter, chunk, visual, job.reason, job.targetUuid);
            }) || changed;
        }

        if (job.processed.length >= job.keypoints.length) {
            depthBoneAllKeypointJobs = depthBoneAllKeypointJobs[0 .. i] ~ depthBoneAllKeypointJobs[i + 1 .. $];
            continue;
        }

        i++;
        if (remaining == 0) break;
    }

    return changed;
}

private bool hasProcessedAllKeypoints(DepthBoneDirtyRequest[] processed, DepthBoneDirtyRequest request) {
    foreach (done; processed) {
        if (done.dirtyScope != DepthBoneDirtyScope.AllKeypoints || done.root !is request.root) continue;
        if (done.actionSink !is request.actionSink) continue;
        if (done.targetUuid != request.targetUuid) continue;
        if (done.parameter is null || done.parameter is request.parameter) return true;
    }
    return false;
}

private bool hasRootAllKeypointsRequest(DepthBoneDirtyRequest[] requests, DepthBoneDirtyRequest request) {
    if (request.parameter is null) return false;
    foreach (candidate; requests) {
        if (candidate.actionSink !is request.actionSink) continue;
        if (candidate.targetUuid != request.targetUuid) continue;
        if (candidate.root is request.root && candidate.parameter is null && candidate.dirtyScope == DepthBoneDirtyScope.AllKeypoints) return true;
    }
    return false;
}

void ngFlushDepthBoneDirty() {
    if (depthBoneDirtyRequests.length > 0) {
        auto requests = depthBoneDirtyRequests;
        depthBoneDirtyRequests.length = 0;
        depthBoneDebugLog("[DepthBoneRefresh] flush: requests=%s", requests.length);
        DepthBoneDirtyRequest[] processed;
        foreach (request; requests) {
            if (!isLiveDepthRigRoot(request.root)) continue;
            if (hasRootAllKeypointsRequest(requests, request)) continue;
            if (request.dirtyScope == DepthBoneDirtyScope.Keypoint && hasProcessedAllKeypoints(processed, request)) continue;
            runWithDepthBoneRefreshActionSink(request.actionSink, {
                if (request.dirtyScope == DepthBoneDirtyScope.AllKeypoints) {
                    return ngRefreshDepthBoneDeformAllKeypoints(
                        request.root,
                        request.parameter,
                        request.keypoint,
                        request.reason,
                        request.targetUuid,
                        request.settleBeforeDispatch);
                } else {
                    return ngRefreshDepthBoneDeform(
                        request.root, request.parameter, request.keypoint, request.reason, request.targetUuid);
                }
            });
            processed ~= request;
        }
    }
    processDepthBoneAllKeypointJobs();
    processDepthBoneGpuSubmissionQueue();
    processDepthBoneGpuRefreshJobs();
    processDepthBoneGpuCompletedJobs();
}

bool ngHasPendingDepthBoneRefresh() {
    return depthBoneDirtyRequests.length > 0 ||
        depthBoneAllKeypointJobs.length > 0 ||
        depthBoneGpuSubmissionQueue.length > 0 ||
        depthBoneGpuRefreshJobs.length > 0 ||
        depthBoneGpuCompletedJobs.length > 0 ||
        ngPendingDepthBoneGpuAsyncJobCount() > 0;
}

bool ngHasPendingDepthBoneRefreshForSink(GroupAction sink) {
    foreach (request; depthBoneDirtyRequests) {
        if (request.actionSink is sink) return true;
    }
    foreach (job; depthBoneAllKeypointJobs) {
        if (job.actionSink is sink) return true;
    }
    foreach (job; depthBoneGpuSubmissionQueue) {
        if (job.actionSink is sink) return true;
    }
    foreach (job; depthBoneGpuRefreshJobs) {
        if (job.actionSink is sink) return true;
    }
    foreach (job; depthBoneGpuCompletedJobs) {
        if (job.actionSink is sink) return true;
    }
    return false;
}

size_t ngPendingDepthBoneRefreshWorkForSink(GroupAction sink) {
    size_t result;
    foreach (request; depthBoneDirtyRequests) {
        if (request.actionSink is sink) result++;
    }
    foreach (job; depthBoneAllKeypointJobs) {
        if (job.actionSink !is sink) continue;
        result += job.keypoints.length > job.processed.length ? job.keypoints.length - job.processed.length : 1;
    }
    foreach (job; depthBoneGpuSubmissionQueue) {
        if (job.actionSink is sink) result++;
    }
    foreach (job; depthBoneGpuRefreshJobs) {
        if (job.actionSink is sink) result++;
    }
    foreach (job; depthBoneGpuCompletedJobs) {
        if (job.actionSink is sink) result++;
    }
    return result;
}

void ngFlushDepthBoneDirtyImmediate() {
    size_t guard;
    while (ngHasPendingDepthBoneRefresh()) {
        ngFlushDepthBoneDirty();
        guard++;
        enforce(guard < 10000, "Depth bone refresh queue did not drain");
    }
}

private void applyRuleJson(ref ExDepthInfluenceRule rule, string text) {
    auto json = parseJSON(text);
    if ("maxInfluences" in json.object) rule.maxInfluences = cast(uint)json["maxInfluences"].integer;
    if ("radiusScale" in json.object) rule.radiusScale = cast(float)json["radiusScale"].floating;
    if ("minimumRadius" in json.object) rule.minimumRadius = cast(float)json["minimumRadius"].floating;
    if ("falloff" in json.object) rule.falloff = json["falloff"].str;
    if ("multipliersByBoneUuid" in json.object) {
        rule.multipliersByBoneUuid.clear();
        foreach (key, value; json["multipliersByBoneUuid"].object) {
            rule.multipliersByBoneUuid[key.to!ulong] = cast(float)value.floating;
        }
    }
    if (rule.maxInfluences == 0) rule.maxInfluences = 1;
}

private void applySourceSettingsJson(ref ExDepthBoneSourceSettings setting, string text) {
    auto json = parseJSON(text);
    if ("weight" in json.object) setting.weight = jsonNumber(json["weight"], setting.weight);
    if ("depthOffset" in json.object) setting.depthOffset = jsonNumber(json["depthOffset"], setting.depthOffset);
    if ("depthScale" in json.object) setting.depthScale = jsonNumber(json["depthScale"], setting.depthScale);
    if ("rotation" in json.object) setting.rotation = jsonNumber(json["rotation"], setting.rotation);
    if (!setting.weight.isFinite) setting.weight = 1.0f;
    if (!setting.depthOffset.isFinite) setting.depthOffset = 0.0f;
    if (!setting.depthScale.isFinite) setting.depthScale = 1.0f;
    setting.rotation = normalizeDepthBoneSourceRotation(setting.rotation);
    if (setting.weight < 0) setting.weight = 0;
    if (setting.depthScale < 0.01f) setting.depthScale = 0.01f;
}

@EffectCreate
class CreateDepthRigRootCommand : ExCommand!(
    TW!(Node, "parent", "Parent node"),
    TW!(string, "name", "Depth rig root name")
) {
    this() { super(_("Create Depth Rig Root"), _("Create DepthRigRoot node")); }

    override CreateResult!Node run(Context ctx) {
        auto actualParent = parent;
        if (actualParent is null && ctx.hasNodes && ctx.nodes.length > 0) actualParent = ctx.nodes[0];
        if (actualParent is null && ctx.hasPuppet && ctx.puppet !is null) actualParent = ctx.puppet.root;
        enforce(actualParent !is null, "No parent node");

        auto root = new ExDepthRigRoot(actualParent);
        root.name = name.length ? name : "DepthRig";
        if (actualParent.puppet) actualParent.puppet.rescanNodes();
        root.notifyChange(root, NotifyReason.StructureChanged);
        return new CreateResult!Node(true, [root]);
    }
}

@EffectCreate
class AddDepthBoneCommand : ExCommand!(
    TW!(Node, "parent", "DepthRigRoot or DepthBone parent"),
    TW!(string, "boneId", "Bone id"),
    TW!(float[], "restHead", "Rest head [x,y,z]"),
    TW!(float[], "restTail", "Rest tail [x,y,z]"),
    TW!(float, "restRoll", "Rest roll")
) {
    this() { super(_("Add Depth Bone"), _("Create DepthBone node")); }

    override CreateResult!Node run(Context ctx) {
        enforce(parent !is null, "parent is required");
        enforce(cast(ExDepthRigRoot)parent || cast(ExDepthBone)parent, "parent must be DepthRigRoot or DepthBone");
        auto bone = ngCreateDepthBone(parent, boneId, vec3From(restHead, "restHead"), vec3From(restTail, "restTail"), restRoll);
        if (parent.puppet) parent.puppet.rescanNodes();
        bone.notifyChange(bone, NotifyReason.StructureChanged);
        return new CreateResult!Node(true, [bone]);
    }
}

@EffectCreate
class AddStandardDepthSkeletonCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(float, "scale", "Template scale")
) {
    this() { super(_("Add Standard Depth Skeleton"), _("Create standard depth bone hierarchy")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        ngAddStandardDepthSkeleton(rigRoot, scale == 0 ? 1.0f : scale);
        if (rigRoot.puppet) rigRoot.puppet.rescanNodes();
        rigRoot.notifyChange(rigRoot, NotifyReason.StructureChanged);
        return CommandResult(true);
    }
}

private struct StandardDepthParameterSpec {
    string name;
    bool isVec2;
    vec2 minValue;
    vec2 maxValue;
    float[] axisX;
    float[] axisY;
}

private struct StandardDepthBindingValue {
    vec2 paramValue;
    float value;
}

private struct StandardDepthBoneBindingSpec {
    string parameterName;
    string boneId;
    string bindingName;
    StandardDepthBindingValue[] values;
}

private StandardDepthParameterSpec[] standardDepthParameterSpecs() {
    auto axis5 = [0.0f, 0.25f, 0.5f, 0.75f, 1.0f];
    return [
        StandardDepthParameterSpec("Face::Yaw-Pitch", true, vec2(-1.0f, -1.0f), vec2(1.0f, 1.0f), axis5.dup, axis5.dup),
        StandardDepthParameterSpec("Face::Roll", false, vec2(-1.0f, 0.0f), vec2(1.0f, 0.0f), axis5.dup, [0.0f]),
        StandardDepthParameterSpec("Body::Yaw-Pitch", true, vec2(-1.0f, -1.0f), vec2(1.0f, 1.0f), axis5.dup, axis5.dup),
        StandardDepthParameterSpec("Body::Roll", false, vec2(-1.0f, 0.0f), vec2(1.0f, 0.0f), axis5.dup, [0.0f]),
    ];
}

private StandardDepthBoneBindingSpec[] standardDepthBoneBindingSpecs() {
    return [
        StandardDepthBoneBindingSpec(
            "Face::Yaw-Pitch",
            "Head",
            "transform.r.y",
            [
                StandardDepthBindingValue(vec2(-1, -1), -0.5235988f),
                StandardDepthBindingValue(vec2(-1,  0), -0.5235988f),
                StandardDepthBindingValue(vec2(-1,  1), -0.5235988f),
                StandardDepthBindingValue(vec2( 0, -1),  0.0f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 0,  1),  0.0f),
                StandardDepthBindingValue(vec2( 1, -1),  0.5235988f),
                StandardDepthBindingValue(vec2( 1,  0),  0.5235988f),
                StandardDepthBindingValue(vec2( 1,  1),  0.5235988f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Face::Yaw-Pitch",
            "Head",
            "transform.r.x",
            [
                StandardDepthBindingValue(vec2(-1, -1), -0.34906584f),
                StandardDepthBindingValue(vec2(-1,  1),  0.34906584f),
                StandardDepthBindingValue(vec2( 0, -1), -0.34906584f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 0,  1),  0.34906584f),
                StandardDepthBindingValue(vec2( 1, -1), -0.34906584f),
                StandardDepthBindingValue(vec2( 1,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  1),  0.34906584f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Face::Roll",
            "Neck",
            "transform.r.z",
            [
                StandardDepthBindingValue(vec2(-1,  0), -0.41800633f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  0),  0.41887903f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Spine",
            "transform.r.y",
            [
                StandardDepthBindingValue(vec2(-1, -1), -0.5235988f),
                StandardDepthBindingValue(vec2(-1,  0), -0.5235988f),
                StandardDepthBindingValue(vec2(-1,  1), -0.5235988f),
                StandardDepthBindingValue(vec2( 0, -1),  0.0f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 0,  1),  0.0f),
                StandardDepthBindingValue(vec2( 1, -1),  0.5235988f),
                StandardDepthBindingValue(vec2( 1,  0),  0.5235988f),
                StandardDepthBindingValue(vec2( 1,  1),  0.5235988f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Spine",
            "transform.r.x",
            [
                StandardDepthBindingValue(vec2(-1, -1), -0.5235988f),
                StandardDepthBindingValue(vec2(-1,  0),  0.0f),
                StandardDepthBindingValue(vec2(-1,  1),  0.17453292f),
                StandardDepthBindingValue(vec2( 0, -1), -0.5235988f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 0,  1),  0.17453292f),
                StandardDepthBindingValue(vec2( 1, -1), -0.5235988f),
                StandardDepthBindingValue(vec2( 1,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  1),  0.17453292f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Chest",
            "transform.r.x",
            [
                StandardDepthBindingValue(vec2(-1, -1), 0.2617994f),
                StandardDepthBindingValue(vec2(-1,  0), 0.0f),
                StandardDepthBindingValue(vec2(-1,  1), 0.34906584f),
                StandardDepthBindingValue(vec2( 0, -1), 0.2617994f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
                StandardDepthBindingValue(vec2( 0,  1), 0.34906584f),
                StandardDepthBindingValue(vec2( 1, -1), 0.2617994f),
                StandardDepthBindingValue(vec2( 1,  0), 0.0f),
                StandardDepthBindingValue(vec2( 1,  1), 0.34906584f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Chest",
            "transform.r.y",
            [
                StandardDepthBindingValue(vec2(-1, -1), 0.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
                StandardDepthBindingValue(vec2( 1,  1), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Clavicle.L",
            "transform.r.x",
            [
                StandardDepthBindingValue(vec2(-1,  1), 0.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Pelvis",
            "transform.t.y",
            [
                StandardDepthBindingValue(vec2( 0, -1), 26.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Shin.L",
            "transform.t.y",
            [
                StandardDepthBindingValue(vec2( 0, -1), 93.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Shin.R",
            "transform.t.y",
            [
                StandardDepthBindingValue(vec2( 0, -1), 93.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Shin.L",
            "transform.t.x",
            [
                StandardDepthBindingValue(vec2( 0, -1), 68.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Yaw-Pitch",
            "Shin.R",
            "transform.t.x",
            [
                StandardDepthBindingValue(vec2( 0, -1), -67.0f),
                StandardDepthBindingValue(vec2( 0,  0), 0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Pelvis",
            "transform.t.x",
            [
                StandardDepthBindingValue(vec2(-1,  0),  30.0f),
                StandardDepthBindingValue(vec2( 0,  0),   0.0f),
                StandardDepthBindingValue(vec2( 1,  0), -30.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Pelvis",
            "transform.t.y",
            [
                StandardDepthBindingValue(vec2(-1,  0), -28.0f),
                StandardDepthBindingValue(vec2( 0,  0),   0.0f),
                StandardDepthBindingValue(vec2( 1,  0), -28.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Pelvis",
            "transform.r.z",
            [
                StandardDepthBindingValue(vec2(-1,  0), -0.049741887f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  0),  0.049741887f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Spine",
            "transform.r.z",
            [
                StandardDepthBindingValue(vec2(-1,  0), -0.15271631f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  0),  0.15271631f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Chest",
            "transform.r.y",
            [
                StandardDepthBindingValue(vec2(-1,  0), -0.0f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  0),  0.0f),
            ]
        ),
        StandardDepthBoneBindingSpec(
            "Body::Roll",
            "Chest",
            "transform.r.z",
            [
                StandardDepthBindingValue(vec2(-1,  0), -0.123918384f),
                StandardDepthBindingValue(vec2( 0,  0),  0.0f),
                StandardDepthBindingValue(vec2( 1,  0),  0.123918384f),
            ]
        ),
    ];
}

private Parameter findDepthParameterByName(string name, bool isVec2) {
    auto puppet = incActivePuppet();
    if (puppet is null) return null;
    foreach (param; puppet.parameters) {
        if (param.name == name && param.isVec2 == isVec2) return param;
    }
    return null;
}

private float parameterAxisValue(Parameter param, size_t axis, size_t index) {
    auto minValue = axis == 0 ? param.min.x : param.min.y;
    auto maxValue = axis == 0 ? param.max.x : param.max.y;
    auto span = maxValue - minValue;
    if (abs(span) <= 0.000001f) return minValue;
    return minValue + span * param.axisPoints[axis][index];
}

private bool parameterStructureMatchesSpec(Parameter param, ref StandardDepthParameterSpec spec) {
    if (param is null) return false;
    if (param.isVec2 != spec.isVec2) return false;
    if (abs(param.min.x - spec.minValue.x) > 0.000001f || abs(param.max.x - spec.maxValue.x) > 0.000001f) return false;
    if (spec.isVec2 && (abs(param.min.y - spec.minValue.y) > 0.000001f || abs(param.max.y - spec.maxValue.y) > 0.000001f)) return false;
    return true;
}

private bool parameterHasAxisValue(Parameter param, uint axis, float value) {
    enum float eps = 0.000001f;
    if (!param.isVec2 && axis == 1)
        return abs(value) <= eps;
    foreach (i; 0 .. param.axisPoints[axis].length) {
        if (abs(parameterAxisValue(param, axis, i) - value) <= eps)
            return true;
    }
    return false;
}

private void ensureParameterAxisValue(Parameter param, uint axis, float value) {
    enum float eps = 0.000001f;
    if (!param.isVec2 && axis == 1) return;
    if (parameterHasAxisValue(param, axis, value)) return;

    auto minValue = axis == 0 ? param.min.x : param.min.y;
    auto maxValue = axis == 0 ? param.max.x : param.max.y;
    auto span = maxValue - minValue;
    if (abs(span) <= eps) return;
    auto offset = (value - minValue) / span;
    if (offset > eps && offset < 1.0f - eps)
        param.insertAxisPoint(axis, offset);
}

private bool standardDepthParameterHasRequiredKeys(Parameter param, StandardDepthBoneBindingSpec[] bindingSpecs) {
    foreach (bindingSpec; bindingSpecs) {
        foreach (valueSpec; bindingSpec.values) {
            if (!parameterHasAxisValue(param, 0, valueSpec.paramValue.x)) return false;
            if (!parameterHasAxisValue(param, 1, valueSpec.paramValue.y)) return false;
        }
    }
    return true;
}

private void ensureStandardDepthParameterKeys(Parameter param, StandardDepthBoneBindingSpec[] bindingSpecs) {
    foreach (bindingSpec; bindingSpecs) {
        foreach (valueSpec; bindingSpec.values) {
            ensureParameterAxisValue(param, 0, valueSpec.paramValue.x);
            ensureParameterAxisValue(param, 1, valueSpec.paramValue.y);
        }
    }
}

private bool conformStandardDepthParameter(Parameter param, ref StandardDepthParameterSpec spec, StandardDepthBoneBindingSpec[] bindingSpecs, GroupAction group, out bool changed, out string message) {
    changed = false;
    if (param.isVec2 != spec.isVec2) {
        message = _("Existing parameter '%s' is %s but the standard depth template requires %s").format(
            spec.name,
            param.isVec2 ? "2D" : "1D",
            spec.isVec2 ? "2D" : "1D"
        );
        return false;
    }
    if (parameterStructureMatchesSpec(param, spec) && standardDepthParameterHasRequiredKeys(param, bindingSpecs))
        return true;

    auto action = new ParameterShapeChangeAction("standard depth parameter axes", param);
    param.min = spec.minValue;
    param.max = spec.maxValue;
    ensureStandardDepthParameterKeys(param, bindingSpecs);
    action.updateNewState();
    group.addAction(action);
    changed = true;
    return true;
}

private ExDepthBone findStandardDepthBone(ExDepthRigRoot root, string boneId) {
    foreach (bone; root.depthBones()) {
        if (bone.boneId == boneId || bone.name == boneId) return bone;
    }
    return null;
}

@EffectBindingEdit
class AddStandardDepthParametersCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node")
) {
    this() { super(_("Add Standard Depth Parameters"), _("Create standard face/body parameters and depth bone bindings")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto puppet = incActivePuppet();
        enforce(puppet !is null, "No active puppet");

        auto bindingSpecs = standardDepthBoneBindingSpecs();
        ExDepthBone[string] bonesById;
        foreach (bindingSpec; bindingSpecs) {
            if (bindingSpec.boneId in bonesById) continue;
            auto bone = findStandardDepthBone(rigRoot, bindingSpec.boneId);
            enforce(bone !is null, _("Missing depth bone '%s'").format(bindingSpec.boneId));
            bonesById[bindingSpec.boneId] = bone;
        }

        auto group = new GroupAction();
        Parameter[string] paramsByName;
        StandardDepthBoneBindingSpec[][string] bindingSpecsByParameter;
        foreach (bindingSpec; bindingSpecs)
            bindingSpecsByParameter[bindingSpec.parameterName] ~= bindingSpec;
        bool changed = false;

        foreach (spec; standardDepthParameterSpecs()) {
            auto param = findDepthParameterByName(spec.name, spec.isVec2);
            if (param is null) {
                auto created = new ExParameter(spec.name, spec.isVec2);
                created.min = spec.minValue;
                created.max = spec.maxValue;
                created.defaults = vec2(0.0f, 0.0f);
                created.value = created.defaults;
                created.axisPoints[0] = spec.axisX.dup;
                created.axisPoints[1] = spec.axisY.dup;
                puppet.parameters ~= created;
                group.addAction(new ParameterAddAction(created, &puppet.parameters));
                param = created;
                changed = true;
            } else {
                string message;
                bool parameterChanged;
                if (!conformStandardDepthParameter(param, spec, bindingSpecsByParameter.get(spec.name, null), group, parameterChanged, message))
                    return CommandResult(false, message);
                changed = changed || parameterChanged;
            }
            paramsByName[spec.name] = param;
        }

        foreach (bindingSpec; bindingSpecs) {
            auto param = paramsByName.get(bindingSpec.parameterName, null);
            enforce(param !is null, _("Missing parameter '%s'").format(bindingSpec.parameterName));
            auto bone = bonesById[bindingSpec.boneId];

            ValueParameterBinding binding = cast(ValueParameterBinding)param.getBinding(bone, bindingSpec.bindingName);
            if (binding is null) {
                binding = cast(ValueParameterBinding)param.createBinding(bone, bindingSpec.bindingName);
                param.addBinding(binding);
                group.addAction(new ParameterBindingAddAction(param, binding));
                changed = true;
            }
            enforce(binding !is null, _("Cannot create value binding '%s'").format(bindingSpec.bindingName));

            foreach (valueSpec; bindingSpec.values) {
                ptrdiff_t xIndex = -1;
                ptrdiff_t yIndex = -1;
                foreach (x; 0 .. param.axisPoints[0].length) {
                    if (abs(parameterAxisValue(param, 0, x) - valueSpec.paramValue.x) <= 0.000001f) {
                        xIndex = cast(ptrdiff_t)x;
                        break;
                    }
                }
                foreach (y; 0 .. param.axisPoints[1].length) {
                    auto axisValue = param.isVec2 ? parameterAxisValue(param, 1, y) : 0.0f;
                    if (abs(axisValue - valueSpec.paramValue.y) <= 0.000001f) {
                        yIndex = cast(ptrdiff_t)y;
                        break;
                    }
                }
                enforce(xIndex >= 0 && yIndex >= 0, _("Standard depth binding keypoint is not present in parameter '%s'").format(bindingSpec.parameterName));
                auto x = cast(size_t)xIndex;
                auto y = cast(size_t)yIndex;
                auto value = valueSpec.value;
                if (binding.isSet_[x][y] && abs(binding.values[x][y] - value) <= 0.000001f) continue;
                auto action = new ParameterBindingValueChangeAction!(float, ValueParameterBinding)(binding.getName(), binding, cast(uint)x, cast(uint)y);
                binding.setValue(vec2u(cast(uint)x, cast(uint)y), value);
                action.updateNewState();
                group.addAction(action);
                changed = true;
            }
            binding.reInterpolate();
        }

        if (!changed) return CommandResult(false, "Standard depth parameters already exist");
        incActionPush(group);
        if (rigRoot.puppet) rigRoot.puppet.rescanNodes();
        return CommandResult(true);
    }
}

@McpTool
@EffectConfigEdit
class FitDepthRigRootZToDepthCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node whose descendant DepthBones will be fitted to mapped depth")
) {
    this() {
        super(
            _("Fit DepthRigRoot Z to Depth"),
            _("Fit descendant DepthBone translation Z values to their current mapped depths"));
    }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        if (!ngFitDepthRigNodeTranslationZToCurrentDepth(rigRoot)) {
            return CommandResult(false, "No usable mapped depth samples were found for the DepthRigRoot");
        }
        return CommandResult(true);
    }
}

@McpTool
@EffectConfigEdit
class FitDepthBoneZToDepthCommand : ExCommand!(
    TW!(Node, "bone", "DepthBone node to fit to its current mapped depth")
) {
    this() {
        super(
            _("Fit DepthBone Z to Depth"),
            _("Fit one DepthBone translation Z value to its current mapped depth"));
    }

    override CommandResult run(Context ctx) {
        auto depthBone = requireBone(bone);
        if (!ngFitDepthRigNodeTranslationZToCurrentDepth(depthBone)) {
            return CommandResult(false, "No usable mapped depth sample was found for the DepthBone");
        }
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class SetDepthBoneRestCommand : ExCommand!(
    TW!(Node, "bone", "DepthBone node"),
    TW!(float[], "restHead", "Rest head [x,y,z]"),
    TW!(float[], "restTail", "Rest tail [x,y,z]"),
    TW!(float, "restRoll", "Rest roll")
) {
    this() { super(_("Set Depth Bone Rest"), _("Set rest pose for a depth bone")); }

    override CommandResult run(Context ctx) {
        auto b = requireBone(bone);
        auto oldHead = b.restHead;
        auto oldTail = b.restTail;
        auto oldRoll = b.restRoll;
        b.restHead = vec3From(restHead, "restHead");
        b.restTail = vec3From(restTail, "restTail");
        b.restRoll = restRoll;
        incActionPush(new DepthBoneRestChangeAction(b, oldHead, oldTail, oldRoll, b.restHead, b.restTail, b.restRoll));
        return CommandResult(true);
    }
}

@EffectConfigEdit
class SetDepthBoneConstraintCommand : ExCommand!(
    TW!(Node, "bone", "DepthBone node"),
    TW!(string, "constraint", "Constraint JSON")
) {
    this() { super(_("Set Depth Bone Constraint"), _("Set depth bone constraint")); }

    override CommandResult run(Context ctx) {
        auto b = requireBone(bone);
        auto action = new DepthBoneConstraintChangeAction(b);
        auto json = parseJSON(constraint);
        if ("constraintType" in json.object) b.constraintType = json["constraintType"].str;
        if ("lockRotation" in json.object) b.lockRotation = json["lockRotation"].boolean;
        if ("lockTranslation" in json.object) b.lockTranslation = json["lockTranslation"].boolean;
        if ("allowParentToTargets" in json.object) b.allowParentToTargets = json["allowParentToTargets"].boolean;
        if ("hingeAxis" in json.object) {
            auto values = json["hingeAxis"].array;
            enforce(values.length == 3, "hingeAxis must be [x,y,z]");
            b.hingeAxis = vec3(jsonNumber(values[0], b.hingeAxis.x), jsonNumber(values[1], b.hingeAxis.y), jsonNumber(values[2], b.hingeAxis.z));
        }
        if ("rotationLimits" in json.object) {
            b.rotationLimits.length = 0;
            foreach (value; json["rotationLimits"].array) b.rotationLimits ~= jsonNumber(value, 0);
        }
        if ("maxStepRadians" in json.object) b.maxStepRadians = jsonNumber(json["maxStepRadians"], b.maxStepRadians);
        action.updateNewState();
        incActionPush(action);
        return CommandResult(true);
    }
}

@ShortcutHidden
class ListDepthBonesCommand : ExCommand!(TW!(Node, "root", "DepthRigRoot node")) {
    this() { super(_("List Depth Bones"), _("List depth bones under root")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto rigRoot = requireRoot(root);
        JSONValue[] items;
        foreach (bone; rigRoot.depthBones()) items ~= boneToJson(bone);
        JSONValue[string] obj;
        obj["items"] = JSONValue(items);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectConfigEdit
class AddDepthBoneSourceCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(Node, "bone", "DepthBone source")
) {
    this() { super(_("Add Depth Bone Source"), _("Add depth bone source to target binding")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto source = requireBone(bone);
        auto oldBindings = rigRoot.bindings.dup;
        rigRoot.addBoneSource(target, targetKindOf(target), source);
        incActionPush(new DepthBoneSourceListChangeAction("Add Depth Bone Source", rigRoot, oldBindings, rigRoot.bindings));
        return CommandResult(true);
    }
}

@EffectConfigEdit
class RemoveDepthBoneSourceCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(Node, "bone", "DepthBone source")
) {
    this() { super(_("Remove Depth Bone Source"), _("Remove depth bone source from target binding")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto oldBindings = rigRoot.bindings.dup;
        rigRoot.removeBoneSource(target, requireBone(bone));
        incActionPush(new DepthBoneSourceListChangeAction("Remove Depth Bone Source", rigRoot, oldBindings, rigRoot.bindings));
        return CommandResult(true);
    }
}

@ShortcutHidden
class ListDepthBoneSourcesCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target")
) {
    this() { super(_("List Depth Bone Sources"), _("List depth bone source UUIDs for target")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto rigRoot = requireRoot(root);
        JSONValue[] sources;
        auto index = rigRoot.findBindingIndex(target.uuid);
        if (index >= 0) {
            foreach (uuid; rigRoot.bindings[cast(size_t)index].sourceBoneUuids) sources ~= JSONValue(uuid);
        }
        JSONValue[] sourceObjects;
        if (index >= 0) {
            auto binding = &rigRoot.bindings[cast(size_t)index];
            binding.normalizeSourceSettings();
            foreach (uuid; binding.sourceBoneUuids) {
                sourceObjects ~= sourceSettingsToJson(binding.sourceSetting(uuid));
            }
        }
        JSONValue[string] obj;
        obj["target"] = JSONValue(target.uuid);
        obj["sourceBoneUuids"] = JSONValue(sources);
        obj["sources"] = JSONValue(sourceObjects);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectConfigEdit
class SetDepthBoneSourceSettingsCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(Node, "bone", "DepthBone source"),
    TW!(string, "settings", "Source settings JSON")
) {
    this() { super(_("Set Depth Bone Source Settings"), _("Set per-source depth bone settings")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto source = requireBone(bone);
        auto oldBindings = rigRoot.bindings.dup;
        auto binding = rigRoot.getOrCreateBinding(target, targetKindOf(target));
        auto setting = binding.sourceSetting(source.uuid);
        setting.boneUuid = source.uuid;
        applySourceSettingsJson(setting, settings);
        binding.setSourceSetting(setting);
        depthBoneDebugLog("[DepthBoneRefresh] source settings command: root=%s target=%s bone=%s weight=%s depthOffset=%s depthScale=%s rotation=%s",
            rigRoot.name,
            target is null ? "(null)" : target.name,
            source.name,
            setting.weight,
            setting.depthOffset,
            setting.depthScale,
            setting.rotation);
        incActionPush(new DepthBoneSourceListChangeAction(
            "Set Depth Bone Source Settings",
            rigRoot,
            oldBindings,
            rigRoot.bindings,
            true));
        return CommandResult(true);
    }
}

@EffectConfigEdit
class SetDepthBoneInfluenceRuleCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(string, "rule", "Influence rule JSON")
) {
    this() { super(_("Set Depth Bone Influence Rule"), _("Set depth bone influence rule")); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto oldBindings = rigRoot.bindings.dup;
        auto binding = rigRoot.getOrCreateBinding(target, targetKindOf(target));
        applyRuleJson(binding.influenceRule, rule);
        incActionPush(new DepthBoneBindingRuleChangeAction("Set Depth Bone Influence Rule", rigRoot, oldBindings, rigRoot.bindings));
        return CommandResult(true);
    }
}

@ShortcutHidden
class GetDepthBoneInfluenceRuleCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target")
) {
    this() { super(_("Get Depth Bone Influence Rule"), _("Get depth bone influence rule")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto binding = rigRoot.getOrCreateBinding(target, targetKindOf(target));
        return ExCommandResult!JSONValue(true, ruleToJson(binding.influenceRule));
    }
}

class PreviewDepthBoneInfluenceCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(Node, "bone", "DepthBone source")
) {
    this() { super(_("Preview Depth Bone Influence"), _("Preview depth bone influence")); }
    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        targetKindOf(target);
        auto source = requireBone(bone);
        auto deformable = cast(Deformable)target;
        enforce(deformable !is null, "target is not deformable");
        auto binding = rigRoot.getOrCreateBinding(target, targetKindOf(target));
        deformable.deformation = generateInfluencePreviewOffsets(binding, source, deformable);
        deformable.notifyChange(deformable, NotifyReason.AttributeChanged);
        return CommandResult(true);
    }
}

private CommandResult enqueueDepthBoneGpuDeformCommand(
    ExDepthRigRoot rigRoot,
    Parameter param,
    vec2u kp,
    Node[] actualTargets,
    string reason,
    bool writePreview,
    bool writeBinding
) {
    enforceDepthBoneGpuAvailable(reason);
    DepthBoneGpuOffsetPacket[] packets;
    foreach (targetNode; actualTargets) {
        targetKindOf(targetNode);
        auto deformable = cast(Deformable)targetNode;
        if (deformable is null) continue;
        auto bindingIndex = rigRoot.findBindingIndex(targetNode.uuid);
        if (bindingIndex < 0) continue;
        DepthBoneGpuOffsetPacket packet;
        string error;
        if (!ngBuildDepthBoneGpuOffsetPacket(
            rigRoot,
            &rigRoot.bindings[cast(size_t)bindingIndex],
            deformable,
            param,
            kp,
            packet,
            error,
            writePreview,
            writeBinding
        )) {
            auto message = "Depth Bone GPU packet build failed: target=%s key=(%s,%s) reason=%s".format(
                targetNode is null ? "(null)" : targetNode.name,
                kp.x,
                kp.y,
                error);
            writeDepthBoneGpuFatalLog(message);
            enforce(false, message);
        }
        packets ~= packet;
    }
    enforce(packets.length > 0, "No depth bone GPU targets updated");
    enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
    return CommandResult(true);
}

class PreviewDepthBoneDeformCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node[], "targets", "GridDeformer or PathDeformer targets")
) {
    this() { super(_("Preview Depth Bone Deform"), _("Preview depth bone deformation")); }
    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto param = ctx.hasArmedParameters && ctx.armedParameters.length > 0 ? ctx.armedParameters[0] : incArmedParameter();
        enforce(param is null || depthBoneParameterDrivesRig(rigRoot, param),
            "Armed parameter does not drive this DepthRigRoot");
        auto kp = param !is null ? param.findClosestKeypoint() : vec2u.init;
        Node[] actualTargets = targets;
        if (actualTargets is null || actualTargets.length == 0) {
            foreach (ref binding; rigRoot.bindings) {
                if (auto node = incActivePuppet().find!Node(cast(uint)binding.targetUuid)) {
                    actualTargets ~= node;
                }
            }
        }
        enforce(actualTargets.length > 0, "No targets");

        return enqueueDepthBoneGpuDeformCommand(
            rigRoot, param, kp, actualTargets, "Preview Depth Bone Deform", true, false);
    }
}

@EffectBindingEdit
class ApplyDepthBoneDeformCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node[], "targets", "GridDeformer or PathDeformer targets")
) {
    this() { super(_("Apply Depth Bone Deform"), _("Apply depth bone deformation to current key")); }
    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto param = ctx.hasArmedParameters && ctx.armedParameters.length > 0 ? ctx.armedParameters[0] : incArmedParameter();
        if (param is null) return CommandResult(false, "No armed parameter");
        enforce(depthBoneParameterDrivesRig(rigRoot, param),
            "Armed parameter does not drive this DepthRigRoot");
        auto kp = param.findClosestKeypoint();

        Node[] actualTargets = targets;
        if (actualTargets is null || actualTargets.length == 0) {
            foreach (ref binding; rigRoot.bindings) {
                if (auto node = incActivePuppet().find!Node(cast(uint)binding.targetUuid)) {
                    actualTargets ~= node;
                }
            }
        }
        enforce(actualTargets.length > 0, "No targets");

        return enqueueDepthBoneGpuDeformCommand(
            rigRoot, param, kp, actualTargets, "Apply Depth Bone Deform", false, true);
    }
}

void ngInitCommands(T)() if (is(T == DepthBoneCommand)) {
    auto createRoot = new CreateDepthRigRootCommand();
    ngRegisterCommandMeta(createRoot);
    commands[DepthBoneCommand.CreateDepthRigRoot] = createRoot;

    auto addBone = new AddDepthBoneCommand();
    ngRegisterCommandMeta(addBone);
    commands[DepthBoneCommand.AddDepthBone] = addBone;

    auto addStandard = new AddStandardDepthSkeletonCommand();
    ngRegisterCommandMeta(addStandard);
    commands[DepthBoneCommand.AddStandardDepthSkeleton] = addStandard;

    auto addStandardParams = new AddStandardDepthParametersCommand();
    ngRegisterCommandMeta(addStandardParams);
    commands[DepthBoneCommand.AddStandardDepthParameters] = addStandardParams;

    auto fitRootZ = new FitDepthRigRootZToDepthCommand();
    ngRegisterCommandMeta(fitRootZ);
    commands[DepthBoneCommand.FitDepthRigRootZToDepth] = fitRootZ;

    auto fitBoneZ = new FitDepthBoneZToDepthCommand();
    ngRegisterCommandMeta(fitBoneZ);
    commands[DepthBoneCommand.FitDepthBoneZToDepth] = fitBoneZ;

    auto setRest = new SetDepthBoneRestCommand();
    ngRegisterCommandMeta(setRest);
    commands[DepthBoneCommand.SetDepthBoneRest] = setRest;

    auto setConstraint = new SetDepthBoneConstraintCommand();
    ngRegisterCommandMeta(setConstraint);
    commands[DepthBoneCommand.SetDepthBoneConstraint] = setConstraint;

    auto listBones = new ListDepthBonesCommand();
    ngRegisterCommandMeta(listBones);
    commands[DepthBoneCommand.ListDepthBones] = listBones;

    auto addSource = new AddDepthBoneSourceCommand();
    ngRegisterCommandMeta(addSource);
    commands[DepthBoneCommand.AddDepthBoneSource] = addSource;

    auto removeSource = new RemoveDepthBoneSourceCommand();
    ngRegisterCommandMeta(removeSource);
    commands[DepthBoneCommand.RemoveDepthBoneSource] = removeSource;

    auto listSources = new ListDepthBoneSourcesCommand();
    ngRegisterCommandMeta(listSources);
    commands[DepthBoneCommand.ListDepthBoneSources] = listSources;

    auto setSourceSettings = new SetDepthBoneSourceSettingsCommand();
    ngRegisterCommandMeta(setSourceSettings);
    commands[DepthBoneCommand.SetDepthBoneSourceSettings] = setSourceSettings;

    auto setRule = new SetDepthBoneInfluenceRuleCommand();
    ngRegisterCommandMeta(setRule);
    commands[DepthBoneCommand.SetDepthBoneInfluenceRule] = setRule;

    auto getRule = new GetDepthBoneInfluenceRuleCommand();
    ngRegisterCommandMeta(getRule);
    commands[DepthBoneCommand.GetDepthBoneInfluenceRule] = getRule;

    auto previewInfluence = new PreviewDepthBoneInfluenceCommand();
    ngRegisterCommandMeta(previewInfluence);
    commands[DepthBoneCommand.PreviewDepthBoneInfluence] = previewInfluence;

    auto previewDeform = new PreviewDepthBoneDeformCommand();
    ngRegisterCommandMeta(previewDeform);
    commands[DepthBoneCommand.PreviewDepthBoneDeform] = previewDeform;

    auto applyDeform = new ApplyDepthBoneDeformCommand();
    ngRegisterCommandMeta(applyDeform);
    commands[DepthBoneCommand.ApplyDepthBoneDeform] = applyDeform;
}
