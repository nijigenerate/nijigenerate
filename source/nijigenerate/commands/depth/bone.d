module nijigenerate.commands.depth.bone;

import nijigenerate.commands.base;
import nijigenerate.actions;
import nijigenerate.actions.binding : ngDepthBoneBindingValueChangeHook;
import nijigenerate.actions.parameter : ParameterChangeBindingsValueAction, ParameterShapeChangeAction;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.ext : ExParameter;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.project : incActivePuppet, incArmedParameter;
import nijigenerate.viewport.depth.mesheditor.node : DepthDisplayPlaneSize, DepthDisplayZScale;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import nijilive.core.param.binding : DeformationParameterBinding, ParameterBinding, ValueParameterBinding;
import nijilive.math;
import i18n;

import std.algorithm.comparison : max, min;
import std.algorithm.sorting : sort;
import std.algorithm.searching : countUntil;
import std.exception : enforce;
import std.json : JSONType, JSONValue, parseJSON;
import std.math : abs, exp, isFinite, sqrt;
import std.conv : to;
import std.string : format, split, startsWith;

private enum EnableDepthBoneDebugLog = false;
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
    ngDepthBoneBindingValueChangeHook = &ngDepthBoneBindingValueChanged;
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

private float distanceSqPointSegment(vec3 p, vec3 a, vec3 b) {
    auto ab = b - a;
    auto ap = p - a;
    auto lenSq = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (lenSq <= 1e-8f) {
        auto d = p - a;
        return d.x * d.x + d.y * d.y + d.z * d.z;
    }
    auto t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / lenSq;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    auto c = a + ab * t;
    auto d = p - c;
    return d.x * d.x + d.y * d.y + d.z * d.z;
}

private float pointSegmentProjection(vec3 p, vec3 a, vec3 b) {
    auto ab = b - a;
    auto ap = p - a;
    auto lenSq = dotVec(ab, ab);
    if (lenSq <= 1e-8f) return 0.0f;
    return dotVec(ap, ab) / lenSq;
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

private float depthAt(Node target, size_t index) {
    if (auto mapped = cast(DepthMappedNode)target) {
        auto depths = mapped.copyDepths();
        if (depths !is null && index < depths.length) return depths[index];
    }
    return 0.0f;
}

private vec2 copyVertex2(Deformable target, size_t index) {
    auto vertex = target.vertices[index];
    return vec2(vertex.x, vertex.y);
}

private float depthScaleFor(Deformable target) {
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
    return max(1.0f, max(size.x, size.y) * (DepthDisplayZScale / DepthDisplayPlaneSize));
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

private void depthScaleDetails(Deformable target, out vec2 minPoint, out vec2 maxPoint, out vec2 size, out float rawScale, out float scale) {
    if (target is null || target.vertices.length == 0) {
        minPoint = vec2(0);
        maxPoint = vec2(0);
        size = vec2(0);
        rawScale = 1.0f;
        scale = 1.0f;
        return;
    }

    minPoint = copyVertex2(target, 0);
    maxPoint = minPoint;
    foreach (i, vertex; target.vertices) {
        if (i == 0) continue;
        minPoint.x = min(minPoint.x, vertex.x);
        minPoint.y = min(minPoint.y, vertex.y);
        maxPoint.x = max(maxPoint.x, vertex.x);
        maxPoint.y = max(maxPoint.y, vertex.y);
    }
    size = maxPoint - minPoint;
    rawScale = max(size.x, size.y) * (DepthDisplayZScale / DepthDisplayPlaneSize);
    scale = max(1.0f, rawScale);
}

private float worldDepthAt(Deformable target, size_t index) {
    return -depthAt(target, index) * depthScaleFor(target);
}

private float worldDepthAt(Deformable target, size_t index, ExDepthBoneSourceSettings setting) {
    auto adjustedDepth = depthAt(target, index) * setting.depthScale + setting.depthOffset;
    return -adjustedDepth * depthScaleFor(target);
}

private void logDepthBoneDepthInput(Node targetNode, Deformable target) {
    auto mapped = cast(DepthMappedNode)targetNode;
    if (mapped is null) {
        depthBoneDebugLog("[DepthBone] %s: depth input skipped: target is not DepthMappedNode",
            targetNode is null ? "(null)" : targetNode.name);
        return;
    }

    auto depths = mapped.copyDepths();
    if (depths is null) {
        depthBoneDebugLog("[DepthBone] %s: depth input: depths=null vertices=%s",
            targetNode is null ? "(null)" : targetNode.name,
            target is null ? 0 : target.vertices.length);
        return;
    }

    size_t nonZero;
    float minDepth = depths.length ? depths[0] : 0;
    float maxDepth = depths.length ? depths[0] : 0;
    ptrdiff_t firstNonZero = -1;
    foreach (i, value; depths) {
        if (value < minDepth) minDepth = value;
        if (value > maxDepth) maxDepth = value;
        if (value < -0.000001f || value > 0.000001f) {
            nonZero++;
            if (firstNonZero < 0) firstNonZero = cast(ptrdiff_t)i;
        }
    }

    vec2 boundsMin;
    vec2 boundsMax;
    vec2 boundsSize;
    float rawScale;
    float scale;
    depthScaleDetails(target, boundsMin, boundsMax, boundsSize, rawScale, scale);

    depthBoneDebugLog("[DepthBone] %s: depth input: depths=%s vertices=%s nonZero=%s min=%s max=%s firstNonZero=%s boundsMin=(%s,%s) boundsMax=(%s,%s) boundsSize=(%s,%s) rawScale=%s scale=%s",
        targetNode is null ? "(null)" : targetNode.name,
        depths.length,
        target is null ? 0 : target.vertices.length,
        nonZero,
        minDepth,
        maxDepth,
        firstNonZero,
        boundsMin.x,
        boundsMin.y,
        boundsMax.x,
        boundsMax.y,
        boundsSize.x,
        boundsSize.y,
        rawScale,
        scale);

    if (target !is null && target.vertices.length > 0) {
        size_t[] samples;
        samples ~= 0;
        samples ~= target.vertices.length / 2;
        samples ~= target.vertices.length - 1;
        if (firstNonZero >= 0) samples ~= cast(size_t)firstNonZero;
        foreach (sample; samples) {
            if (sample >= target.vertices.length) continue;
            auto vertex = target.vertices[sample];
            auto depth = sample < depths.length ? depths[sample] : 0.0f;
            depthBoneDebugLog("[DepthBone] %s: depth sample[%s]: vertex=(%s,%s) depth=%s scaledZ=%s",
                targetNode is null ? "(null)" : targetNode.name,
                sample,
                vertex.x,
                vertex.y,
                depth,
                -depth * scale);
        }
    }
}

private quat depthEditRotation(float pitch, float yaw, float roll) {
    return quat.eulerRotation(pitch, yaw, roll);
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

private void effectiveDepthBoneRest(ExDepthRigRoot root, ExDepthBone bone, out vec3 head, out vec3 tail) {
    head = depthBoneNodeRestPosition(root, bone);
    if (auto childBone = firstDepthBoneChild(bone)) {
        tail = depthBoneNodeRestPosition(root, childBone);
        return;
    }
    if (auto parentBone = cast(ExDepthBone)bone.parent) {
        auto parentPoint = depthBoneNodeRestPosition(root, parentBone);
        auto direction = head - parentPoint;
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f) direction = bone.restTail - bone.restHead;
        if (segmentLength(vec3(0, 0, 0), direction) <= 1e-4f) direction = vec3(0, 100, 0);
        tail = head + direction;
        return;
    }

    auto fallback = bone.restTail - bone.restHead;
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
    vec3 worldHead;
    vec3 worldTail;
    quat worldQuaternion;
    mat4 bindMatrix;
    mat4 inverseBindMatrix;
    mat4 skinMatrix;
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
            rb.poseQuaternion = bone.lockRotation
                ? quat.identity
                : depthEditRotation(
                    parameterValue(bone, param, cursor, "transform.r.x", 0),
                    parameterValue(bone, param, cursor, "transform.r.y", 0),
                    parameterValue(bone, param, cursor, "transform.r.z", 0)
                );
        } else {
            vec3 restLocalTranslation;
            if (auto parentBone = cast(ExDepthBone)bone.parent) {
                auto parentRest = parentBone.uuid in runtime;
                restLocalTranslation = parentRest is null ? rb.restHead : rb.restHead - (*parentRest).restHead;
            } else {
                restLocalTranslation = rb.restHead;
            }
            rb.poseTranslation = bone.lockTranslation ? vec3(0, 0, 0) : bone.localTransform.translation - restLocalTranslation;
            rb.poseQuaternion = bone.lockRotation ? quat.identity : depthEditRotation(bone.localTransform.rotation.x, bone.localTransform.rotation.y, bone.localTransform.rotation.z);
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
            rb.worldQuaternion = rb.parent.worldQuaternion * rb.localRestQuaternion * rb.poseQuaternion;
        } else {
            rb.worldHead = rb.restHead + rb.poseTranslation;
            rb.worldQuaternion = rb.restQuaternion * rb.poseQuaternion;
        }
        rb.worldTail = rb.worldHead + (rb.worldQuaternion * vec3(0, rb.restLength, 0));
        rb.skinMatrix = composeMatrix(rb.worldHead, rb.worldQuaternion) * rb.inverseBindMatrix;
    }

    return runtime;
}

private Vec2Array generateDepthBoneOffsets(ExDepthRigRoot root, ExDepthRigBinding* binding, Deformable target, Parameter param = null, vec2u cursor = vec2u.init) {
    enforce(root !is null, "Depth rig root is required");
    enforce(binding !is null, "Depth rig binding is required");
    enforce(target !is null, "target is not deformable");
    enforce(binding.sourceBoneUuids.length > 0, "Depth rig binding has no bone sources");

    ExDepthBone[] bones;
    foreach (uuid; binding.sourceBoneUuids) {
        if (auto bone = findBoneByUuid(root, uuid)) bones ~= bone;
    }
    enforce(bones.length > 0, "No valid depth bone sources");

    struct Influence {
        ExDepthBone bone;
        ExDepthBoneSourceSettings sourceSetting;
        vec3 rest;
        float score;
        float distanceSq;
        float projection;
        float radiusSq;
        float terminalDistanceSq;
        float terminalProjection;
    }

    auto maxInfluences = binding.influenceRule.maxInfluences == 0 ? 1 : binding.influenceRule.maxInfluences;
    auto runtime = buildDepthRigRuntime(root, param, cursor);
    auto targetNode = cast(Node)target;
    auto targetToRoot = targetToRootMatrix(root, targetNode);
    auto rootToTarget = targetToRoot.inverse;
    Vec2Array offsets;
    offsets.length = target.vertices.length;
    logDepthBoneDepthInput(targetNode, target);
    bool loggedDepthTransformSample;
    bool loggedInfluenceSample;
    auto influenceRadiusFloor = max(binding.influenceRule.minimumRadius, targetBoundsSize(target) * 0.18f);

    foreach (i, vertex; target.vertices) {
        auto restLocal = vec3(vertex.x, vertex.y, worldDepthAt(target, i));
        auto rest = transformPoint(targetToRoot, restLocal);
        Influence[] influences;

        foreach (bone; bones) {
            auto setting = binding.sourceSetting(bone.uuid);
            auto multiplier = setting.weight;
            if (auto p = bone.uuid in binding.influenceRule.multipliersByBoneUuid) multiplier *= *p;
            if (!multiplier.isFinite || multiplier <= 0) continue;
            auto restRuntimeBone = bone.uuid in runtime;
            enforce(restRuntimeBone !is null, "Depth bone runtime is missing");
            auto sourceRestLocal = vec3(vertex.x, vertex.y, worldDepthAt(target, i, setting));
            auto sourceRest = transformPoint(targetToRoot, sourceRestLocal);
            auto score = multiplier;
            float distanceSq;
            float projection;
            float radiusSq = float.max;
            float terminalDistanceSq = float.max;
            float terminalProjection;
            if (bones.length > 1) {
                auto radius = max(
                    (*restRuntimeBone).restLength * 0.85f * binding.influenceRule.radiusScale,
                    influenceRadiusFloor
                );
                if (radius <= 1e-6f) radius = 1.0f;
                radiusSq = radius * radius;
                projection = pointSegmentProjection(sourceRest, (*restRuntimeBone).restHead, (*restRuntimeBone).restTail);
                distanceSq = distanceSqPointSegment(sourceRest, (*restRuntimeBone).restHead, (*restRuntimeBone).restTail);
                if (binding.influenceRule.falloff == "linear") {
                    auto distance = cast(float)sqrt(distanceSq);
                    score *= max(0.0f, 1.0f - distance / radius);
                } else {
                    score *= cast(float)exp(-distanceSq / (radius * radius));
                }
                if (isTerminalDepthBoneSource(bone, bones) && (*restRuntimeBone).parent !is null) {
                    terminalProjection = pointSegmentProjection(sourceRest, (*restRuntimeBone).parent.restHead, (*restRuntimeBone).restHead);
                    terminalDistanceSq = distanceSqPointSegment(sourceRest, (*restRuntimeBone).parent.restHead, (*restRuntimeBone).restHead);
                }
            }
            if (!score.isFinite) continue;
            influences ~= Influence(bone, setting, sourceRest, score, distanceSq, projection, radiusSq, terminalDistanceSq, terminalProjection);
        }

        if (influences.length == 0) {
            offsets[i] = vec2(0, 0);
            continue;
        }

        if (bones.length > 1) {
            ptrdiff_t lockedTerminalIndex = -1;
            foreach (j, influence; influences) {
                if (!influence.bone.lockToRoot) continue;
                if (!isTerminalDepthBoneSource(influence.bone, bones)) continue;
                if (influence.terminalProjection <= 1.0f) continue;
                if (influence.terminalDistanceSq > influence.radiusSq) continue;
                if (lockedTerminalIndex < 0) {
                    lockedTerminalIndex = cast(ptrdiff_t)j;
                    continue;
                }
                auto current = influences[cast(size_t)lockedTerminalIndex];
                if (influence.score > current.score || (influence.score == current.score && influence.terminalDistanceSq < current.terminalDistanceSq)) {
                    lockedTerminalIndex = cast(ptrdiff_t)j;
                }
            }
            if (lockedTerminalIndex >= 0) {
                auto terminal = influences[cast(size_t)lockedTerminalIndex];
                terminal.score = 1.0f;
                terminal.distanceSq = 0.0f;
                influences = [terminal];
            }
        }

        sort!((a, b) => a.score == b.score ? a.distanceSq < b.distanceSq : a.score > b.score)(influences);
        if (influences.length > maxInfluences) influences.length = maxInfluences;

        if (!loggedInfluenceSample && bones.length > 1 && (rest.z < -0.000001f || rest.z > 0.000001f)) {
            auto targetNode = cast(Node)target;
            depthBoneDebugLog("[DepthBone] %s: influence sample[%s]: radiusFloor=%s maxInfluences=%s candidates=%s",
                targetNode is null ? "(null)" : targetNode.name,
                i,
                influenceRadiusFloor,
                maxInfluences,
                influences.length);
            foreach (influence; influences) {
                depthBoneDebugLog("[DepthBone] %s: influence sample[%s] bone=%s score=%s distance=%s",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    influence.score,
                    cast(float)sqrt(influence.distanceSq));
            }
            loggedInfluenceSample = true;
        }

        float total = 0;
        foreach (influence; influences) total += influence.score;
        bool usedInfluenceFallback;
        if (total <= 1e-8f) {
            influences.length = 1;
            total = 1.0f;
            influences[0].score = 1.0f;
            usedInfluenceFallback = true;
        }

        if (loggedInfluenceSample && !loggedDepthTransformSample && (rest.z < -0.000001f || rest.z > 0.000001f)) {
            auto targetNode = cast(Node)target;
            depthBoneDebugLog("[DepthBone] %s: normalized influence sample[%s]: total=%s fallback=%s",
                targetNode is null ? "(null)" : targetNode.name,
                i,
                total,
                usedInfluenceFallback);
            foreach (influence; influences) {
                depthBoneDebugLog("[DepthBone] %s: normalized influence sample[%s] bone=%s weight=%s rawScore=%s distance=%s",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    influence.score / total,
                    influence.score,
                    cast(float)sqrt(influence.distanceSq));
            }
        }

        vec3 deformed = vec3(0, 0, 0);
        foreach (influence; influences) {
            auto weight = influence.score / total;
            auto runtimeBone = influence.bone.uuid in runtime;
            enforce(runtimeBone !is null, "Depth bone runtime is missing");
            deformed += transformPoint((*runtimeBone).skinMatrix, influence.rest) * weight;
        }
        auto deformedLocal = transformPoint(rootToTarget, deformed);
        offsets[i] = vec2(deformedLocal.x - vertex.x, deformedLocal.y - vertex.y);
        if (!loggedDepthTransformSample && (rest.z < -0.000001f || rest.z > 0.000001f)) {
            depthBoneDebugLog("[DepthBone] %s: transform sample[%s]: restLocal=(%s,%s,%s) restRoot=(%s,%s,%s) deformedRoot=(%s,%s,%s) deformedLocal=(%s,%s,%s) offset=(%s,%s) totalWeight=%s influences=%s",
                targetNode is null ? "(null)" : targetNode.name,
                i,
                restLocal.x,
                restLocal.y,
                restLocal.z,
                rest.x,
                rest.y,
                rest.z,
                deformed.x,
                deformed.y,
                deformed.z,
                deformedLocal.x,
                deformedLocal.y,
                deformedLocal.z,
                offsets[i].x,
                offsets[i].y,
                total,
                influences.length);
            foreach (influence; influences) {
                auto runtimeBone = influence.bone.uuid in runtime;
                if (runtimeBone is null) continue;
                depthBoneDebugLog("[DepthBone] %s: transform sample[%s] influence bone=%s score=%s weight=%s head=(%s,%s,%s) tail=(%s,%s,%s)",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    influence.score,
                    influence.score / total,
                    (*runtimeBone).worldHead.x,
                    (*runtimeBone).worldHead.y,
                    (*runtimeBone).worldHead.z,
                    (*runtimeBone).worldTail.x,
                    (*runtimeBone).worldTail.y,
                    (*runtimeBone).worldTail.z);
                depthBoneDebugLog("[DepthBone] %s: transform sample[%s] source bone=%s sourceWeight=%s depthOffset=%s depthScale=%s sourceRestRoot=(%s,%s,%s)",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    influence.sourceSetting.weight,
                    influence.sourceSetting.depthOffset,
                    influence.sourceSetting.depthScale,
                    influence.rest.x,
                    influence.rest.y,
                    influence.rest.z);
                depthBoneDebugLog("[DepthBone] %s: transform sample[%s] bind bone=%s restHead=(%s,%s,%s) restTail=(%s,%s,%s) localRestOffset=(%s,%s,%s) poseTranslation=(%s,%s,%s) poseQuaternion=(%s,%s,%s,%s) worldQuaternion=(%s,%s,%s,%s)",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    (*runtimeBone).restHead.x,
                    (*runtimeBone).restHead.y,
                    (*runtimeBone).restHead.z,
                    (*runtimeBone).restTail.x,
                    (*runtimeBone).restTail.y,
                    (*runtimeBone).restTail.z,
                    (*runtimeBone).localRestOffset.x,
                    (*runtimeBone).localRestOffset.y,
                    (*runtimeBone).localRestOffset.z,
                    (*runtimeBone).poseTranslation.x,
                    (*runtimeBone).poseTranslation.y,
                    (*runtimeBone).poseTranslation.z,
                    (*runtimeBone).poseQuaternion.w,
                    (*runtimeBone).poseQuaternion.x,
                    (*runtimeBone).poseQuaternion.y,
                    (*runtimeBone).poseQuaternion.z,
                    (*runtimeBone).worldQuaternion.w,
                    (*runtimeBone).worldQuaternion.x,
                    (*runtimeBone).worldQuaternion.y,
                    (*runtimeBone).worldQuaternion.z);
            }
            loggedDepthTransformSample = true;
        }
    }

    return offsets;
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
    DepthBoneDirtyScope dirtyScope;
    string reason;
    GroupAction actionSink;
}

private DepthBoneDirtyRequest[] depthBoneDirtyRequests;
private ExDepthRigRoot lastDepthBoneDirtyRoot;
private Parameter lastDepthBoneDirtyParameter;
private vec2u lastDepthBoneDirtyKeypoint;
private GroupAction depthBoneRefreshActionSink;

private enum size_t DepthBoneAllKeypointsPerFrame = 4;

private struct DepthBoneAllKeypointJob {
    ExDepthRigRoot root;
    Parameter parameter;
    vec2u[] keypoints;
    bool[string] processed;
    size_t nextIndex;
    string reason;
    GroupAction actionSink;
}

private DepthBoneAllKeypointJob[] depthBoneAllKeypointJobs;

private struct DepthBoneFingerprint {
    ulong rigHash;
    ulong[uint] parameterStructureHashes;
    ulong[string] poseKeyHashes;
}

private DepthBoneFingerprint[uint] depthBoneFingerprints;

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

private string dirtyScopeName(DepthBoneDirtyScope dirtyScope) {
    return dirtyScope == DepthBoneDirtyScope.AllKeypoints ? "all-keypoints" : "keypoint";
}

private bool sameDirtyParameter(DepthBoneDirtyRequest request, ExDepthRigRoot root, Parameter parameter) {
    return request.root is root && request.parameter is parameter && request.actionSink is depthBoneRefreshActionSink;
}

void ngMarkDepthBoneDirty(
    ExDepthRigRoot root,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope.Keypoint,
) {
    if (root is null) return;
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
        if (!sameDirtyParameter(request, root, parameter)) continue;
        if (request.dirtyScope == DepthBoneDirtyScope.AllKeypoints || dirtyScope == DepthBoneDirtyScope.AllKeypoints) {
            request.dirtyScope = DepthBoneDirtyScope.AllKeypoints;
            request.keypoint = keypoint;
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
    depthBoneDirtyRequests ~= DepthBoneDirtyRequest(root, parameter, keypoint, dirtyScope, reason, depthBoneRefreshActionSink);
}

void ngMarkDepthBoneDirtyForArmedParameter(
    ExDepthRigRoot root,
    string reason,
    DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope.Keypoint,
) {
    auto param = incArmedParameter();
    auto keypoint = param is null ? vec2u.init : param.findClosestKeypoint();
    if (param is null && lastDepthBoneDirtyRoot is root && lastDepthBoneDirtyParameter !is null) {
        param = lastDepthBoneDirtyParameter;
        keypoint = lastDepthBoneDirtyKeypoint;
    }
    ngMarkDepthBoneDirty(root, param, keypoint, reason, dirtyScope);
}

void ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(ExDepthRigRoot root, string reason) {
    ngMarkDepthBoneDirtyForArmedParameter(root, reason, DepthBoneDirtyScope.AllKeypoints);
}

void ngMarkDepthBoneDirtyForTarget(Node target, string reason) {
    if (target is null || incActivePuppet() is null) return;
    auto param = incArmedParameter();
    auto keypoint = param is null ? vec2u.init : param.findClosestKeypoint();

    if (auto bone = cast(ExDepthBone)target) {
        if (auto root = findDepthRigRoot(bone)) {
            auto actualParam = param;
            auto actualKeypoint = keypoint;
            if (actualParam is null && lastDepthBoneDirtyRoot is root && lastDepthBoneDirtyParameter !is null) {
                actualParam = lastDepthBoneDirtyParameter;
                actualKeypoint = lastDepthBoneDirtyKeypoint;
            }
            if (actualParam is null) {
                ngMarkDepthBoneDirty(root, null, actualKeypoint, reason, DepthBoneDirtyScope.AllKeypoints);
            } else {
                ngMarkDepthBoneDirty(root, actualParam, actualKeypoint, reason, DepthBoneDirtyScope.Keypoint);
            }
        }
        return;
    }

    void visit(Node node) {
        if (node is null) return;
        if (auto root = cast(ExDepthRigRoot)node) {
            if (root.findBindingIndex(target.uuid) >= 0) {
                auto actualParam = param;
                auto actualKeypoint = keypoint;
                if (actualParam is null && lastDepthBoneDirtyRoot is root && lastDepthBoneDirtyParameter !is null) {
                    actualParam = lastDepthBoneDirtyParameter;
                    actualKeypoint = lastDepthBoneDirtyKeypoint;
                }
                ngMarkDepthBoneDirty(root, actualParam, actualKeypoint, reason, DepthBoneDirtyScope.AllKeypoints);
            }
        }
        foreach (child; node.children) visit(child);
    }

    visit(incActivePuppet().root);
}

bool ngAutoApplyDepthBoneDeform(ExDepthBone changedBone, Parameter param, vec2u kp) {
    if (changedBone is null || param is null) return false;
    auto rigRoot = findDepthRigRoot(changedBone);
    if (rigRoot is null) return false;
    ngMarkDepthBoneDirty(rigRoot, param, kp, "Depth Bone Transform");
    return true;
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

void ngDepthBoneBindingValueChanged(Parameter param, Node target, string bindingName, vec2u kp) {
    if (target is null || param is null || !isDepthBoneTransformBindingName(bindingName)) return;
    if (auto bone = cast(ExDepthBone)target) {
        ngAutoApplyDepthBoneDeform(bone, param, kp);
        return;
    }
    ngMarkDepthBoneDirtyForTransformBindingTarget(target, param, kp, "Depth Bone Target Transform");
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

private bool rootContainsBone(ExDepthRigRoot root, ExDepthBone bone) {
    if (root is null || bone is null) return false;
    Node cursor = bone;
    while (cursor !is null) {
        if (cursor is root) return true;
        cursor = cursor.parent;
    }
    return false;
}

private string poseKey(uint parameterUuid, uint x, uint y) {
    return parameterUuid.to!string ~ ":" ~ x.to!string ~ ":" ~ y.to!string;
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
            if (auto depthMapped = cast(DepthMappedNode)targetNode) {
                auto depths = depthMapped.copyDepths();
                hash = hashInt(hash, depths is null ? -1 : cast(long)depths.length);
                if (depths !is null) foreach (depth; depths) hash = hashFloat(hash, depth);
            } else {
                hash = hashInt(hash, -2);
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

private DepthBoneFingerprint computeDepthBoneFingerprint(ExDepthRigRoot root) {
    DepthBoneFingerprint fingerprint;
    fingerprint.rigHash = depthBoneRigStructureHash(root);
    auto puppet = incActivePuppet();
    if (root is null || puppet is null) return fingerprint;

    foreach (param; puppet.parameters) {
        if (param is null) continue;
        auto paramHash = depthBoneParameterStructureHash(root, param);
        bool hasDepthBoneBinding;
        foreach (binding; param.bindings) {
            auto bone = cast(ExDepthBone)binding.getTarget().node;
            if (bone is null || !rootContainsBone(root, bone)) continue;
            if (!isDepthBoneTransformBindingName(binding.getName())) continue;
            hasDepthBoneBinding = true;
            break;
        }
        if (!hasDepthBoneBinding) continue;
        fingerprint.parameterStructureHashes[param.uuid] = paramHash;
        foreach (kp; depthBoneKeypoints(param)) {
            fingerprint.poseKeyHashes[poseKey(param.uuid, kp.x, kp.y)] = depthBonePoseKeyHash(root, param, kp);
        }
    }

    return fingerprint;
}

private void ngCheckDepthBoneFingerprints() {
    bool[uint] liveRootUuids;
    foreach (root; depthBoneRoots()) {
        liveRootUuids[cast(uint)root.uuid] = true;
        auto current = computeDepthBoneFingerprint(root);
        auto oldPtr = cast(uint)root.uuid in depthBoneFingerprints;
        if (oldPtr is null) {
            depthBoneFingerprints[cast(uint)root.uuid] = current;
            continue;
        }
        auto old = *oldPtr;
        if (current.rigHash != old.rigHash) {
            ngMarkDepthBoneDirty(root, null, vec2u.init, "Depth Bone Dependency", DepthBoneDirtyScope.AllKeypoints);
        }

        foreach (paramUuid, hash; current.parameterStructureHashes) {
            auto oldHash = paramUuid in old.parameterStructureHashes;
            if (oldHash is null || *oldHash != hash) {
                if (auto param = incActivePuppet().findParameter(paramUuid)) {
                    ngMarkDepthBoneDirty(root, param, param.findClosestKeypoint(), "Depth Bone Parameter Structure", DepthBoneDirtyScope.AllKeypoints);
                }
            }
        }

        foreach (key, hash; current.poseKeyHashes) {
            auto oldHash = key in old.poseKeyHashes;
            if (oldHash is null || *oldHash != hash) {
                auto parts = key.split(":");
                if (parts.length == 3) {
                    auto paramUuid = parts[0].to!uint;
                    if (auto param = incActivePuppet().findParameter(paramUuid)) {
                        auto kp = vec2u(parts[1].to!uint, parts[2].to!uint);
                        ngMarkDepthBoneDirty(root, param, kp, "Depth Bone Pose", DepthBoneDirtyScope.Keypoint);
                    }
                }
            }
        }

        depthBoneFingerprints[cast(uint)root.uuid] = current;
    }

    uint[] staleRootUuids;
    foreach (rootUuid; depthBoneFingerprints.byKey) {
        if (rootUuid !in liveRootUuids) staleRootUuids ~= rootUuid;
    }
    foreach (rootUuid; staleRootUuids) depthBoneFingerprints.remove(rootUuid);
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

    bool hasDepthBonePoseBinding(Parameter param) {
        foreach (binding; param.bindings) {
            auto bone = cast(ExDepthBone)binding.getTarget().node;
            if (bone is null || !rootContainsBone(rigRoot, bone)) continue;
            if (isDepthBoneTransformBindingName(binding.getName())) return true;
        }
        return false;
    }

    auto armed = incArmedParameter();
    if (armed !is null) result ~= armed;
    if (lastDepthBoneDirtyRoot is rigRoot && lastDepthBoneDirtyParameter !is null && !hasParam(lastDepthBoneDirtyParameter)) {
        result ~= lastDepthBoneDirtyParameter;
    }

    foreach (param; puppet.parameters) {
        if (param is null || hasParam(param)) continue;
        if (hasDepthBonePoseBinding(param)) {
            result ~= param;
            continue;
        }
        foreach (ref binding; rigRoot.bindings) {
            auto targetNode = puppet.find!Node(cast(uint)binding.targetUuid);
            if (targetNode is null) continue;
            if (param.getBinding(targetNode, "deform") !is null) {
                result ~= param;
                break;
            }
        }
    }
    return result;
}

private bool ngRefreshDepthBoneDeform(ExDepthRigRoot rigRoot, Parameter param, vec2u kp, string reason) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (rigRoot is null || incActivePuppet() is null) return false;
    depthBoneDebugLog("[DepthBoneRefresh] refresh start: root=%s param=%s key=(%s,%s) reason=%s bindings=%s",
        rigRoot.name,
        param is null ? "(none)" : param.name,
        kp.x,
        kp.y,
        reason,
        rigRoot.bindings.length);

    DeformationParameterBinding[] deformBindings;
    Vec2Array[] offsetsList;
    ParameterBinding[] created;

    foreach (ref binding; rigRoot.bindings) {
        if (!hasValidDepthBoneSources(rigRoot, binding)) continue;
        auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
        auto deformable = cast(Deformable)targetNode;
        if (deformable is null) continue;

        auto offsets = generateDepthBoneOffsets(rigRoot, &binding, deformable, param, kp);
        size_t nonZero;
        foreach (offset; offsets) {
            if (offset.x < -0.0001f || offset.x > 0.0001f || offset.y < -0.0001f || offset.y > 0.0001f) nonZero++;
        }
        depthBoneDebugLog("[DepthBoneRefresh] target refreshed: target=%s offsets=%s nonZero=%s writeBinding=%s",
            targetNode is null ? "(null)" : targetNode.name,
            offsets.length,
            nonZero,
            param !is null);
        deformable.deformation = offsets;
        deformable.notifyChange(deformable, NotifyReason.AttributeChanged);

        if (param is null) continue;
        auto existing = param.getBinding(targetNode, "deform");
        auto deformBinding = cast(DeformationParameterBinding)existing;
        if (deformBinding is null) {
            deformBinding = cast(DeformationParameterBinding)param.getOrAddBinding(targetNode, "deform");
            created ~= deformBinding;
        }
        if (deformBinding is null) continue;
        deformBindings ~= deformBinding;
        offsetsList ~= offsets;
    }

    if (param is null) {
        depthBoneDebugLog("[DepthBoneRefresh] refresh done: preview-only root=%s", rigRoot.name);
        return true;
    }
    if (deformBindings.length == 0) return false;

    auto group = new GroupAction();
    foreach (binding; created) group.addAction(new ParameterBindingAddAction(param, binding));

    auto label = reason.length ? _("Auto Refresh Depth Bone Deform: %s").format(reason) : _("Auto Refresh Depth Bone Deform");
    auto action = new ParameterChangeBindingsValueAction(label, param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
    foreach (i, binding; deformBindings) {
        binding.update(kp, offsetsList[i]);
    }
    action.updateNewState();
    group.addAction(action);
    pushDepthBoneRefreshAction(group);
    depthBoneDebugLog("[DepthBoneRefresh] refresh done: root=%s bindings=%s", rigRoot.name, deformBindings.length);
    return true;
}

private bool ngRefreshDepthBoneDeformKeypoints(ExDepthRigRoot rigRoot, Parameter param, vec2u[] keypoints, vec2u visualKeypoint, string reason) {
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

    DeformationParameterBinding[] deformBindings;
    Deformable[] deformables;
    Node[] targetNodes;
    ExDepthRigBinding*[] rigBindings;
    ParameterBinding[] created;

    foreach (ref binding; rigRoot.bindings) {
        if (!hasValidDepthBoneSources(rigRoot, binding)) continue;
        auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
        auto deformable = cast(Deformable)targetNode;
        if (deformable is null) continue;

        auto existing = param.getBinding(targetNode, "deform");
        auto deformBinding = cast(DeformationParameterBinding)existing;
        if (deformBinding is null) {
            deformBinding = cast(DeformationParameterBinding)param.getOrAddBinding(targetNode, "deform");
            created ~= deformBinding;
        }
        if (deformBinding is null) continue;
        deformBindings ~= deformBinding;
        deformables ~= deformable;
        targetNodes ~= targetNode;
        rigBindings ~= &binding;
    }

    if (deformBindings.length == 0) return false;

    auto group = new GroupAction();
    foreach (binding; created) group.addAction(new ParameterBindingAddAction(param, binding));

    auto label = reason.length ? _("Auto Refresh Depth Bone Deform: %s").format(reason) : _("Auto Refresh Depth Bone Deform");
    foreach (kp; keypoints) {
        Vec2Array[] offsetsList;
        offsetsList.length = deformBindings.length;

        foreach (i, deformable; deformables) {
            auto offsets = generateDepthBoneOffsets(rigRoot, rigBindings[i], deformable, param, kp);
            offsetsList[i] = offsets;
            if (kp == visualKeypoint) {
                size_t nonZero;
                foreach (offset; offsets) {
                    if (offset.x < -0.0001f || offset.x > 0.0001f || offset.y < -0.0001f || offset.y > 0.0001f) nonZero++;
                }
                depthBoneDebugLog("[DepthBoneRefresh] target refreshed: target=%s offsets=%s nonZero=%s writeBinding=true scope=all-keypoints visual=true",
                    targetNodes[i] is null ? "(null)" : targetNodes[i].name,
                    offsets.length,
                    nonZero);
                deformable.deformation = offsets;
                deformable.notifyChange(deformable, NotifyReason.AttributeChanged);
            }
        }

        auto action = new ParameterChangeBindingsValueAction(label, param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
        foreach (i, binding; deformBindings) binding.update(kp, offsetsList[i]);
        action.updateNewState();
        group.addAction(action);
    }

    pushDepthBoneRefreshAction(group);
    depthBoneDebugLog("[DepthBoneRefresh] refresh chunk done: root=%s bindings=%s keypoints=%s scope=all-keypoints",
        rigRoot.name,
        deformBindings.length,
        keypoints.length);
    return true;
}

private bool enqueueDepthBoneAllKeypoints(ExDepthRigRoot rigRoot, Parameter param, string reason) {
    if (!isLiveDepthRigRoot(rigRoot)) return false;
    if (rigRoot is null || param is null) return false;
    auto keypoints = depthBoneKeypoints(param);
    if (keypoints.length == 0) return false;

    foreach (ref job; depthBoneAllKeypointJobs) {
        if (job.root is rigRoot && job.parameter is param && job.actionSink is depthBoneRefreshActionSink) {
            job.keypoints = keypoints;
            job.processed.clear();
            job.nextIndex = 0;
            job.reason = reason;
            return true;
        }
    }

    DepthBoneAllKeypointJob job;
    job.root = rigRoot;
    job.parameter = param;
    job.keypoints = keypoints;
    job.reason = reason;
    job.actionSink = depthBoneRefreshActionSink;
    depthBoneAllKeypointJobs ~= job;
    return true;
}

private bool ngRefreshDepthBoneDeformAllKeypoints(ExDepthRigRoot rigRoot, Parameter param, vec2u currentKeypoint, string reason) {
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
            queued = enqueueDepthBoneAllKeypoints(rigRoot, resolvedParam, reason) || queued;
        }
        return queued;
    }
    return enqueueDepthBoneAllKeypoints(rigRoot, param, reason);
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
                return ngRefreshDepthBoneDeformKeypoints(job.root, job.parameter, chunk, visual, job.reason);
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
        if (done.parameter is null || done.parameter is request.parameter) return true;
    }
    return false;
}

private bool hasRootAllKeypointsRequest(DepthBoneDirtyRequest[] requests, DepthBoneDirtyRequest request) {
    if (request.parameter is null) return false;
    foreach (candidate; requests) {
        if (candidate.actionSink !is request.actionSink) continue;
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
                    return ngRefreshDepthBoneDeformAllKeypoints(request.root, request.parameter, request.keypoint, request.reason);
                } else {
                    return ngRefreshDepthBoneDeform(request.root, request.parameter, request.keypoint, request.reason);
                }
            });
            processed ~= request;
        }
    }
    processDepthBoneAllKeypointJobs();
}

bool ngHasPendingDepthBoneRefresh() {
    return depthBoneDirtyRequests.length > 0 || depthBoneAllKeypointJobs.length > 0;
}

bool ngHasPendingDepthBoneRefreshForSink(GroupAction sink) {
    foreach (request; depthBoneDirtyRequests) {
        if (request.actionSink is sink) return true;
    }
    foreach (job; depthBoneAllKeypointJobs) {
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
    if (!setting.weight.isFinite) setting.weight = 1.0f;
    if (!setting.depthOffset.isFinite) setting.depthOffset = 0.0f;
    if (!setting.depthScale.isFinite) setting.depthScale = 1.0f;
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
        if (auto rigRoot = findDepthRigRoot(b)) ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(rigRoot, "Depth Bone Rest");
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
        if (auto rigRoot = findDepthRigRoot(b)) ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(rigRoot, "Depth Bone Constraint");
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
        ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(rigRoot, "Add Depth Bone Source");
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
        ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(rigRoot, "Remove Depth Bone Source");
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
        depthBoneDebugLog("[DepthBoneRefresh] source settings command: root=%s target=%s bone=%s weight=%s depthOffset=%s depthScale=%s",
            rigRoot.name,
            target is null ? "(null)" : target.name,
            source.name,
            setting.weight,
            setting.depthOffset,
            setting.depthScale);
        incActionPush(new DepthBoneSourceListChangeAction("Set Depth Bone Source Settings", rigRoot, oldBindings, rigRoot.bindings));
        auto param = ctx.hasArmedParameters && ctx.armedParameters.length > 0 ? ctx.armedParameters[0] : null;
        if (param !is null) {
            ngMarkDepthBoneDirty(rigRoot, param, param.findClosestKeypoint(), "Depth Bone Source Settings", DepthBoneDirtyScope.AllKeypoints);
        } else {
            ngMarkDepthBoneDirty(rigRoot, null, vec2u.init, "Depth Bone Source Settings", DepthBoneDirtyScope.AllKeypoints);
        }
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
        ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(rigRoot, "Depth Bone Influence Rule");
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

class PreviewDepthBoneDeformCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node[], "targets", "GridDeformer or PathDeformer targets")
) {
    this() { super(_("Preview Depth Bone Deform"), _("Preview depth bone deformation")); }
    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto param = ctx.hasArmedParameters && ctx.armedParameters.length > 0 ? ctx.armedParameters[0] : incArmedParameter();
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

        bool changed = false;
        foreach (targetNode; actualTargets) {
            targetKindOf(targetNode);
            auto deformable = cast(Deformable)targetNode;
            if (deformable is null) continue;
            auto bindingIndex = rigRoot.findBindingIndex(targetNode.uuid);
            if (bindingIndex < 0) continue;
            deformable.deformation = generateDepthBoneOffsets(rigRoot, &rigRoot.bindings[cast(size_t)bindingIndex], deformable, param, kp);
            deformable.notifyChange(deformable, NotifyReason.AttributeChanged);
            changed = true;
        }
        return CommandResult(changed, changed ? "" : "No preview targets updated");
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

        DeformationParameterBinding[] deformBindings;
        Vec2Array[] offsetsList;
        ParameterBinding[] created;

        foreach (targetNode; actualTargets) {
            targetKindOf(targetNode);
            auto deformable = cast(Deformable)targetNode;
            if (deformable is null) continue;
            auto bindingIndex = rigRoot.findBindingIndex(targetNode.uuid);
            if (bindingIndex < 0) continue;
            auto offsets = generateDepthBoneOffsets(rigRoot, &rigRoot.bindings[cast(size_t)bindingIndex], deformable, param, kp);
            auto existing = param.getBinding(targetNode, "deform");
            auto deformBinding = cast(DeformationParameterBinding)existing;
            if (deformBinding is null) {
                deformBinding = cast(DeformationParameterBinding)param.getOrAddBinding(targetNode, "deform");
                created ~= deformBinding;
            }
            if (deformBinding is null) continue;
            deformBindings ~= deformBinding;
            offsetsList ~= offsets;
        }

        enforce(deformBindings.length > 0, "No deform bindings updated");

        auto group = new GroupAction();
        foreach (binding; created) group.addAction(new ParameterBindingAddAction(param, binding));

        auto action = new ParameterChangeBindingsValueAction("Apply Depth Bone Deform", param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
        foreach (i, binding; deformBindings) {
            binding.update(kp, offsetsList[i]);
        }
        action.updateNewState();
        group.addAction(action);
        incActionPush(group);
        return CommandResult(true);
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
