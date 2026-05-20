module nijigenerate.commands.depth.bone;

import nijigenerate.commands.base;
import nijigenerate.actions;
import nijigenerate.actions.parameter : ParameterChangeBindingsValueAction;
import nijigenerate.core.actionstack : incActionPush;
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

import std.algorithm.comparison : max, min;
import std.algorithm.sorting : sort;
import std.algorithm.searching : countUntil;
import std.exception : enforce;
import std.json : JSONType, JSONValue, parseJSON;
import std.math : exp, isFinite, sqrt;
import std.stdio : writefln;
import std.conv : to;

enum DepthBoneCommand {
    CreateDepthRigRoot,
    AddDepthBone,
    AddStandardDepthSkeleton,
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

Command[DepthBoneCommand] commands;

private vec3 vec3From(float[] values, string name) {
    enforce(values.length == 3, name ~ " must be [x, y, z]");
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
        writefln("[DepthBone] %s: depth input skipped: target is not DepthMappedNode",
            targetNode is null ? "(null)" : targetNode.name);
        return;
    }

    auto depths = mapped.copyDepths();
    if (depths is null) {
        writefln("[DepthBone] %s: depth input: depths=null vertices=%s",
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

    writefln("[DepthBone] %s: depth input: depths=%s vertices=%s nonZero=%s min=%s max=%s firstNonZero=%s boundsMin=(%s,%s) boundsMax=(%s,%s) boundsSize=(%s,%s) rawScale=%s scale=%s",
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
            writefln("[DepthBone] %s: depth sample[%s]: vertex=(%s,%s) depth=%s scaledZ=%s",
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
        if (rb.parent !is null) {
            rb.worldHead = rb.parent.worldHead + (rb.parent.worldQuaternion * rb.localRestOffset) + (rb.parent.worldQuaternion * rb.poseTranslation);
            rb.worldQuaternion = rb.parent.worldQuaternion * rb.localRestQuaternion * rb.poseQuaternion;
        } else {
            rb.worldHead = rb.restHead + rb.poseTranslation;
            rb.worldQuaternion = rb.localRestQuaternion * rb.poseQuaternion;
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
            if (bones.length > 1) {
                auto radius = max(
                    (*restRuntimeBone).restLength * 0.85f * binding.influenceRule.radiusScale,
                    influenceRadiusFloor
                );
                if (radius <= 1e-6f) radius = 1.0f;
                distanceSq = distanceSqPointSegment(sourceRest, (*restRuntimeBone).restHead, (*restRuntimeBone).restTail);
                if (binding.influenceRule.falloff == "linear") {
                    auto distance = cast(float)sqrt(distanceSq);
                    score *= max(0.0f, 1.0f - distance / radius);
                } else {
                    score *= cast(float)exp(-distanceSq / (radius * radius));
                }
            }
            if (!score.isFinite) continue;
            influences ~= Influence(bone, setting, sourceRest, score, distanceSq);
        }

        if (influences.length == 0) {
            offsets[i] = vec2(0, 0);
            continue;
        }

        sort!((a, b) => a.score == b.score ? a.distanceSq < b.distanceSq : a.score > b.score)(influences);
        if (influences.length > maxInfluences) influences.length = maxInfluences;

        if (!loggedInfluenceSample && bones.length > 1 && (rest.z < -0.000001f || rest.z > 0.000001f)) {
            auto targetNode = cast(Node)target;
            writefln("[DepthBone] %s: influence sample[%s]: radiusFloor=%s maxInfluences=%s candidates=%s",
                targetNode is null ? "(null)" : targetNode.name,
                i,
                influenceRadiusFloor,
                maxInfluences,
                influences.length);
            foreach (influence; influences) {
                writefln("[DepthBone] %s: influence sample[%s] bone=%s score=%s distance=%s",
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
            writefln("[DepthBone] %s: normalized influence sample[%s]: total=%s fallback=%s",
                targetNode is null ? "(null)" : targetNode.name,
                i,
                total,
                usedInfluenceFallback);
            foreach (influence; influences) {
                writefln("[DepthBone] %s: normalized influence sample[%s] bone=%s weight=%s rawScore=%s distance=%s",
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
            writefln("[DepthBone] %s: transform sample[%s]: restLocal=(%s,%s,%s) restRoot=(%s,%s,%s) deformedRoot=(%s,%s,%s) deformedLocal=(%s,%s,%s) offset=(%s,%s) totalWeight=%s influences=%s",
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
                writefln("[DepthBone] %s: transform sample[%s] influence bone=%s score=%s weight=%s head=(%s,%s,%s) tail=(%s,%s,%s)",
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
                writefln("[DepthBone] %s: transform sample[%s] source bone=%s sourceWeight=%s depthOffset=%s depthScale=%s sourceRestRoot=(%s,%s,%s)",
                    targetNode is null ? "(null)" : targetNode.name,
                    i,
                    influence.bone.name,
                    influence.sourceSetting.weight,
                    influence.sourceSetting.depthOffset,
                    influence.sourceSetting.depthScale,
                    influence.rest.x,
                    influence.rest.y,
                    influence.rest.z);
                writefln("[DepthBone] %s: transform sample[%s] bind bone=%s restHead=(%s,%s,%s) restTail=(%s,%s,%s) localRestOffset=(%s,%s,%s) poseTranslation=(%s,%s,%s) poseQuaternion=(%s,%s,%s,%s) worldQuaternion=(%s,%s,%s,%s)",
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

bool ngAutoApplyDepthBoneDeform(ExDepthBone changedBone, Parameter param, vec2u kp) {
    if (changedBone is null || param is null) return false;
    auto rigRoot = findDepthRigRoot(changedBone);
    if (rigRoot is null || incActivePuppet() is null) return false;

    DeformationParameterBinding[] deformBindings;
    Vec2Array[] offsetsList;
    ParameterBinding[] created;

    foreach (ref binding; rigRoot.bindings) {
        bool affected = false;
        foreach (uuid; binding.sourceBoneUuids) {
            auto source = findBoneByUuid(rigRoot, uuid);
            if (source !is null && isSameOrDescendantBone(source, changedBone)) {
                affected = true;
                break;
            }
        }
        if (!affected) continue;

        auto targetNode = incActivePuppet().find!Node(cast(uint)binding.targetUuid);
        auto deformable = cast(Deformable)targetNode;
        if (deformable is null) continue;

        auto offsets = generateDepthBoneOffsets(rigRoot, &binding, deformable, param, kp);
        auto existing = param.getBinding(targetNode, "deform");
        auto deformBinding = cast(DeformationParameterBinding)existing;
        if (deformBinding is null) {
            deformBinding = cast(DeformationParameterBinding)param.getOrAddBinding(targetNode, "deform");
            created ~= deformBinding;
        }
        if (deformBinding is null) continue;
        deformable.deformation = offsets;
        deformable.notifyChange(deformable, NotifyReason.AttributeChanged);
        deformBindings ~= deformBinding;
        offsetsList ~= offsets;
    }

    if (deformBindings.length == 0) return false;

    auto group = new GroupAction();
    foreach (binding; created) group.addAction(new ParameterBindingAddAction(param, binding));

    auto action = new ParameterChangeBindingsValueAction("Auto Apply Depth Bone Deform", param, cast(ParameterBinding[])deformBindings, cast(int)kp.x, cast(int)kp.y);
    foreach (i, binding; deformBindings) {
        binding.update(kp, offsetsList[i]);
    }
    action.updateNewState();
    group.addAction(action);
    incActionPush(group);
    return true;
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
    this() { super("Create Depth Rig Root", "Create DepthRigRoot node"); }

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
    this() { super("Add Depth Bone", "Create DepthBone node"); }

    override CreateResult!Node run(Context ctx) {
        enforce(parent !is null, "parent is required");
        enforce(cast(ExDepthRigRoot)parent || cast(ExDepthBone)parent, "parent must be DepthRigRoot or DepthBone");
        auto bone = incCreateDepthBone(parent, boneId, vec3From(restHead, "restHead"), vec3From(restTail, "restTail"), restRoll);
        if (parent.puppet) parent.puppet.rescanNodes();
        return new CreateResult!Node(true, [bone]);
    }
}

@EffectCreate
class AddStandardDepthSkeletonCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(float, "scale", "Template scale")
) {
    this() { super("Add Standard Depth Skeleton", "Create standard depth bone hierarchy"); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        incAddStandardDepthSkeleton(rigRoot, scale == 0 ? 1.0f : scale);
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
    this() { super("Set Depth Bone Rest", "Set rest pose for a depth bone"); }

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
    this() { super("Set Depth Bone Constraint", "Set depth bone constraint"); }

    override CommandResult run(Context ctx) {
        auto b = requireBone(bone);
        auto action = new DepthBoneConstraintChangeAction(b);
        auto json = parseJSON(constraint);
        if ("constraintType" in json.object) b.constraintType = json["constraintType"].str;
        if ("lockRotation" in json.object) b.lockRotation = json["lockRotation"].boolean;
        if ("lockTranslation" in json.object) b.lockTranslation = json["lockTranslation"].boolean;
        if ("maxStepRadians" in json.object) b.maxStepRadians = cast(float)json["maxStepRadians"].floating;
        action.updateNewState();
        incActionPush(action);
        return CommandResult(true);
    }
}

@ShortcutHidden
class ListDepthBonesCommand : ExCommand!(TW!(Node, "root", "DepthRigRoot node")) {
    this() { super("List Depth Bones", "List depth bones under root"); }

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
    this() { super("Add Depth Bone Source", "Add depth bone source to target binding"); }

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
    this() { super("Remove Depth Bone Source", "Remove depth bone source from target binding"); }

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
    this() { super("List Depth Bone Sources", "List depth bone source UUIDs for target"); }

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
    this() { super("Set Depth Bone Source Settings", "Set per-source depth bone settings"); }

    override CommandResult run(Context ctx) {
        auto rigRoot = requireRoot(root);
        auto source = requireBone(bone);
        auto oldBindings = rigRoot.bindings.dup;
        auto binding = rigRoot.getOrCreateBinding(target, targetKindOf(target));
        auto setting = binding.sourceSetting(source.uuid);
        setting.boneUuid = source.uuid;
        applySourceSettingsJson(setting, settings);
        binding.setSourceSetting(setting);
        incActionPush(new DepthBoneSourceListChangeAction("Set Depth Bone Source Settings", rigRoot, oldBindings, rigRoot.bindings));
        return CommandResult(true);
    }
}

@EffectConfigEdit
class SetDepthBoneInfluenceRuleCommand : ExCommand!(
    TW!(Node, "root", "DepthRigRoot node"),
    TW!(Node, "target", "GridDeformer or PathDeformer target"),
    TW!(string, "rule", "Influence rule JSON")
) {
    this() { super("Set Depth Bone Influence Rule", "Set depth bone influence rule"); }

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
    this() { super("Get Depth Bone Influence Rule", "Get depth bone influence rule"); }

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
    this() { super("Preview Depth Bone Influence", "Preview depth bone influence"); }
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
    this() { super("Preview Depth Bone Deform", "Preview depth bone deformation"); }
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
    this() { super("Apply Depth Bone Deform", "Apply depth bone deformation to current key"); }
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
