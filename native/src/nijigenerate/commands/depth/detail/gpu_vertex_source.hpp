#pragma once

#include <string_view>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

// Exact project-authored shader text from bone_gpu_async.d.
inline constexpr char VertexSourceBytes[] = R"NG_GPU_GLSL(#version 330

#define MAX_INFLUENCES 16u
#define BONE_STRIDE 24u
#define SOURCE_STRIDE 28u

layout(location = 0) in float inX;
layout(location = 1) in float inY;
layout(location = 2) in float inDepth;

out float outDeformX;
out float outDeformY;

uniform uint vertexCount;
uniform uint boneCount;
uniform uint sourceCount;
uniform uint maxInfluences;
uniform float influenceRadiusFloor;
uniform float radiusScale;
uniform mat4 targetToRoot;
uniform mat4 rootToTarget;
uniform samplerBuffer bones;
uniform samplerBuffer sources;

float boneValue(uint index) {
    return texelFetch(bones, int(index)).r;
}

float sourceValue(uint index) {
    return texelFetch(sources, int(index)).r;
}

vec3 boneRestHead(uint boneIndex) {
    uint base = boneIndex * BONE_STRIDE;
    return vec3(boneValue(base), boneValue(base + 1u), boneValue(base + 2u));
}

float boneRestLength(uint boneIndex) {
    return boneValue(boneIndex * BONE_STRIDE + 3u);
}

vec3 boneRestTail(uint boneIndex) {
    uint base = boneIndex * BONE_STRIDE + 4u;
    return vec3(boneValue(base), boneValue(base + 1u), boneValue(base + 2u));
}

float boneParentIndex(uint boneIndex) {
    return boneValue(boneIndex * BONE_STRIDE + 7u);
}

mat4 boneSkinMatrix(uint boneIndex) {
    uint base = boneIndex * BONE_STRIDE + 8u;
    return mat4(
        boneValue(base + 0u), boneValue(base + 4u), boneValue(base + 8u), boneValue(base + 12u),
        boneValue(base + 1u), boneValue(base + 5u), boneValue(base + 9u), boneValue(base + 13u),
        boneValue(base + 2u), boneValue(base + 6u), boneValue(base + 10u), boneValue(base + 14u),
        boneValue(base + 3u), boneValue(base + 7u), boneValue(base + 11u), boneValue(base + 15u));
}

mat4 sourceNoYawSkinMatrix(uint sourceIndex) {
    uint base = sourceIndex * SOURCE_STRIDE + 12u;
    return mat4(
        sourceValue(base + 0u), sourceValue(base + 4u), sourceValue(base + 8u), sourceValue(base + 12u),
        sourceValue(base + 1u), sourceValue(base + 5u), sourceValue(base + 9u), sourceValue(base + 13u),
        sourceValue(base + 2u), sourceValue(base + 6u), sourceValue(base + 10u), sourceValue(base + 14u),
        sourceValue(base + 3u), sourceValue(base + 7u), sourceValue(base + 11u), sourceValue(base + 15u));
}

float sourceValue(uint sourceIndex, uint component) {
    return sourceValue(sourceIndex * SOURCE_STRIDE + component);
}

float pointSegmentProjection(vec3 p, vec3 a, vec3 b) {
    vec3 ab = b - a;
    float denom = dot(ab, ab);
    if (denom <= 0.00000001) return 0.0;
    return dot(p - a, ab) / denom;
}

float distanceSqPointSegment(vec3 p, vec3 a, vec3 b) {
    float t = clamp(pointSegmentProjection(p, a, b), 0.0, 1.0);
    vec3 q = a + (b - a) * t;
    vec3 d = p - q;
    return dot(d, d);
}

void insertInfluence(
    inout float scores[16],
    inout float distances[16],
    inout uint boneIndices[16],
    inout uint sourceIndices[16],
    inout vec3 rests[16],
    inout uint count,
    float score,
    float distanceSq,
    uint boneIndex,
    uint sourceIndex,
    vec3 rest
) {
    uint limit = min(maxInfluences, MAX_INFLUENCES);
    if (limit == 0u) return;
    uint pos = count;
    if (count < limit) {
        count++;
    } else {
        pos = limit - 1u;
        if (score < scores[pos] || (score == scores[pos] && distanceSq >= distances[pos])) return;
    }
    while (pos > 0u) {
        uint prev = pos - 1u;
        if (score < scores[prev] || (score == scores[prev] && distanceSq >= distances[prev])) break;
        scores[pos] = scores[prev];
        distances[pos] = distances[prev];
        boneIndices[pos] = boneIndices[prev];
        sourceIndices[pos] = sourceIndices[prev];
        rests[pos] = rests[prev];
        pos = prev;
    }
    scores[pos] = score;
    distances[pos] = distanceSq;
    boneIndices[pos] = boneIndex;
    sourceIndices[pos] = sourceIndex;
    rests[pos] = rest;
}

void main() {
    float x = inX;
    float y = inY;
    float z = inDepth;

    float scores[16];
    float distances[16];
    uint boneIndices[16];
    uint sourceIndices[16];
    vec3 rests[16];
    uint influenceCount = 0u;

    int lockedTerminal = -1;
    float lockedScore = 0.0;
    float lockedDistance = 0.0;
    uint lockedBoneIndex = 0u;
    uint lockedSourceIndex = 0u;
    vec3 lockedRest = vec3(0.0);
    for (uint s = 0u; s < sourceCount; ++s) {
        uint boneIndex = uint(sourceValue(s, 0u));
        if (boneIndex >= boneCount) continue;

        float weight = sourceValue(s, 4u);
        float depthScale = sourceValue(s, 5u);
        float depthOffset = sourceValue(s, 6u);
        float multiplier = sourceValue(s, 7u);
        float score = weight * multiplier;
        if (!(score > 0.0)) continue;

        float sourceDepth = z * depthScale + depthOffset;
        vec3 sourceRestLocal = vec3(x, y, sourceDepth);
        vec3 sourceRest = (targetToRoot * vec4(sourceRestLocal, 1.0)).xyz;
        vec3 restHead = boneRestHead(boneIndex);
        vec3 restTail = boneRestTail(boneIndex);
        float restLength = max(boneRestLength(boneIndex), 0.0001);
        float distanceSq = 0.0;
        float terminalDistanceSq = 3.402823e38;
        float terminalProjection = 0.0;
        float radius = max(restLength * 0.85 * radiusScale, influenceRadiusFloor);
        float radiusSq = radius * radius;

        if (sourceCount > 1u) {
            distanceSq = distanceSqPointSegment(sourceRest, restHead, restTail);
            if (uint(sourceValue(s, 3u)) == 1u) {
                float distance = sqrt(distanceSq);
                score *= max(0.0, 1.0 - distance / radius);
            } else {
                score *= exp(-distanceSq / radiusSq);
            }

            if (uint(sourceValue(s, 1u)) != 0u && boneParentIndex(boneIndex) >= 0.0) {
                uint parentIndex = uint(boneParentIndex(boneIndex));
                if (parentIndex < boneCount) {
                    vec3 parentHead = boneRestHead(parentIndex);
                    terminalProjection = pointSegmentProjection(sourceRest, parentHead, restHead);
                    terminalDistanceSq = distanceSqPointSegment(sourceRest, parentHead, restHead);
                }
            }
        }

        if (!(score > 0.0)) continue;

        if (sourceCount > 1u && uint(sourceValue(s, 1u)) != 0u && uint(sourceValue(s, 2u)) != 0u &&
            terminalProjection > 1.0 && terminalDistanceSq <= radiusSq) {
            if (lockedTerminal < 0 || score > lockedScore ||
                (score == lockedScore && terminalDistanceSq < lockedDistance)) {
                lockedTerminal = int(s);
                lockedScore = score;
                lockedDistance = terminalDistanceSq;
                lockedBoneIndex = boneIndex;
                lockedSourceIndex = s;
                lockedRest = sourceRest;
            }
            continue;
        }

        insertInfluence(scores, distances, boneIndices, sourceIndices, rests, influenceCount, score, distanceSq, boneIndex, s, sourceRest);
    }

    if (lockedTerminal >= 0) {
        influenceCount = 1u;
        scores[0] = 1.0;
        boneIndices[0] = lockedBoneIndex;
        sourceIndices[0] = lockedSourceIndex;
        rests[0] = lockedRest;
    }

    if (influenceCount == 0u) {
        outDeformX = 0.0;
        outDeformY = 0.0;
        return;
    }

    float total = 0.0;
    for (uint j = 0u; j < influenceCount; ++j) total += scores[j];
    if (!(total > 0.00000001)) {
        total = 1.0;
        influenceCount = 1u;
        scores[0] = 1.0;
    }

    vec3 rest = vec3(0.0);
    vec3 deformed = vec3(0.0);
    for (uint j = 0u; j < influenceCount; ++j) {
        uint boneIndex = boneIndices[j];
        uint sourceIndex = sourceIndices[j];
        float w = scores[j] / total;
        rest += rests[j] * w;
        mat4 fullSkin = boneSkinMatrix(boneIndex);
        float rotationPivotXShift = sourceValue(sourceIndex, 10u);
        float poseYaw = sourceValue(sourceIndex, 11u);
        float sinRotation = sourceValue(sourceIndex, 8u);
        float cosRotation = sourceValue(sourceIndex, 9u);
        vec3 deformedPoint;
        if (poseYaw == 0.0 || rotationPivotXShift == 0.0 ||
            (sinRotation == 0.0 && cosRotation == 1.0)) {
            deformedPoint = (fullSkin * vec4(rests[j], 1.0)).xyz;
        } else {
            vec3 rotationRest = rests[j] + vec3(rotationPivotXShift, 0.0, 0.0);
            mat4 noYawSkin = sourceNoYawSkinMatrix(sourceIndex);
            vec3 baseDeformed = (noYawSkin * vec4(rests[j], 1.0)).xyz;
            vec3 rotationWithYaw = (fullSkin * vec4(rotationRest, 1.0)).xyz;
            vec3 rotationWithoutYaw = (noYawSkin * vec4(rotationRest, 1.0)).xyz;
            deformedPoint = baseDeformed + rotationWithYaw - rotationWithoutYaw;
        }
        deformed += deformedPoint * w;
    }

    vec3 restLocal = (rootToTarget * vec4(rest, 1.0)).xyz;
    vec3 local = (rootToTarget * vec4(deformed, 1.0)).xyz;
    outDeformX = local.x - restLocal.x;
    outDeformY = local.y - restLocal.y;
}
)NG_GPU_GLSL";
inline constexpr std::string_view VertexSource{VertexSourceBytes, sizeof(VertexSourceBytes) - 1};

}
