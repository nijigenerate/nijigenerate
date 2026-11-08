#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];

    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }

    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }

    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }

    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct Bones
{
    float boneData[1];
};

struct Sources
{
    float sourceData[1];
};

struct Parameters
{
    uint vertexCount;
    uint boneCount;
    uint sourceCount;
    uint maxInfluences;
    float influenceRadiusFloor;
    float radiusScale;
    float2 padding;
    float4x4 targetToRoot;
    float4x4 rootToTarget;
};

struct Dispatch
{
    uint count;
};

struct Positions
{
    float positions[1];
};

struct Depths
{
    float depths[1];
};

struct Output
{
    float result[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(64u, 1u, 1u);

static inline __attribute__((always_inline))
float sourceValue(thread const uint& index, device Sources& _95)
{
    return _95.sourceData[index];
}

static inline __attribute__((always_inline))
float sourceValue(thread const uint& sourceIndex, thread const uint& component, device Sources& _95)
{
    uint param = (sourceIndex * 28u) + component;
    return sourceValue(param, _95);
}

static inline __attribute__((always_inline))
float boneValue(thread const uint& index, device Bones& _83)
{
    return _83.boneData[index];
}

static inline __attribute__((always_inline))
float3 boneRestHead(thread const uint& boneIndex, device Bones& _83)
{
    uint base = boneIndex * 24u;
    uint param = base;
    uint param_1 = base + 1u;
    uint param_2 = base + 2u;
    return float3(boneValue(param, _83), boneValue(param_1, _83), boneValue(param_2, _83));
}

static inline __attribute__((always_inline))
float3 boneRestTail(thread const uint& boneIndex, device Bones& _83)
{
    uint base = (boneIndex * 24u) + 4u;
    uint param = base;
    uint param_1 = base + 1u;
    uint param_2 = base + 2u;
    return float3(boneValue(param, _83), boneValue(param_1, _83), boneValue(param_2, _83));
}

static inline __attribute__((always_inline))
float boneRestLength(thread const uint& boneIndex, device Bones& _83)
{
    uint param = (boneIndex * 24u) + 3u;
    return boneValue(param, _83);
}

static inline __attribute__((always_inline))
float pointSegmentProjection(thread const float3& p, thread const float3& a, thread const float3& b)
{
    float3 ab = b - a;
    float denom = dot(ab, ab);
    if (denom <= 9.9999999392252902907785028219223e-09)
    {
        return 0.0;
    }
    return dot(p - a, ab) / denom;
}

static inline __attribute__((always_inline))
float distanceSqPointSegment(thread const float3& p, thread const float3& a, thread const float3& b)
{
    float3 param = p;
    float3 param_1 = a;
    float3 param_2 = b;
    float t = fast::clamp(pointSegmentProjection(param, param_1, param_2), 0.0, 1.0);
    float3 q = a + ((b - a) * t);
    float3 d = p - q;
    return dot(d, d);
}

static inline __attribute__((always_inline))
float boneParentIndex(thread const uint& boneIndex, device Bones& _83)
{
    uint param = (boneIndex * 24u) + 7u;
    return boneValue(param, _83);
}

static inline __attribute__((always_inline))
void insertInfluence(thread spvUnsafeArray<float, 16>& scores, thread spvUnsafeArray<float, 16>& distances, thread spvUnsafeArray<uint, 16>& boneIndices, thread spvUnsafeArray<uint, 16>& sourceIndices, thread spvUnsafeArray<float3, 16>& rests, thread uint& count, thread const float& score, thread const float& distanceSq, thread const uint& boneIndex, thread const uint& sourceIndex, thread const float3& rest, device Parameters& _382)
{
    uint limit = min(_382.maxInfluences, 16u);
    if (limit == 0u)
    {
        return;
    }
    uint pos = count;
    if (count < limit)
    {
        count++;
    }
    else
    {
        pos = limit - 1u;
        bool _410 = score < scores[pos];
        bool _427;
        if (!_410)
        {
            bool _418 = score == scores[pos];
            bool _426;
            if (_418)
            {
                _426 = distanceSq >= distances[pos];
            }
            else
            {
                _426 = _418;
            }
            _427 = _426;
        }
        else
        {
            _427 = _410;
        }
        if (_427)
        {
            return;
        }
    }
    while (pos > 0u)
    {
        uint prev = pos - 1u;
        bool _445 = score < scores[prev];
        bool _462;
        if (!_445)
        {
            bool _453 = score == scores[prev];
            bool _461;
            if (_453)
            {
                _461 = distanceSq >= distances[prev];
            }
            else
            {
                _461 = _453;
            }
            _462 = _461;
        }
        else
        {
            _462 = _445;
        }
        if (_462)
        {
            break;
        }
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

static inline __attribute__((always_inline))
float4x4 boneSkinMatrix(thread const uint& boneIndex, device Bones& _83)
{
    uint base = (boneIndex * 24u) + 8u;
    uint param = base + 0u;
    uint param_1 = base + 4u;
    uint param_2 = base + 8u;
    uint param_3 = base + 12u;
    uint param_4 = base + 1u;
    uint param_5 = base + 5u;
    uint param_6 = base + 9u;
    uint param_7 = base + 13u;
    uint param_8 = base + 2u;
    uint param_9 = base + 6u;
    uint param_10 = base + 10u;
    uint param_11 = base + 14u;
    uint param_12 = base + 3u;
    uint param_13 = base + 7u;
    uint param_14 = base + 11u;
    uint param_15 = base + 15u;
    return float4x4(float4(boneValue(param, _83), boneValue(param_1, _83), boneValue(param_2, _83), boneValue(param_3, _83)), float4(boneValue(param_4, _83), boneValue(param_5, _83), boneValue(param_6, _83), boneValue(param_7, _83)), float4(boneValue(param_8, _83), boneValue(param_9, _83), boneValue(param_10, _83), boneValue(param_11, _83)), float4(boneValue(param_12, _83), boneValue(param_13, _83), boneValue(param_14, _83), boneValue(param_15, _83)));
}

static inline __attribute__((always_inline))
float4x4 sourceNoYawSkinMatrix(thread const uint& sourceIndex, device Sources& _95)
{
    uint base = (sourceIndex * 28u) + 12u;
    uint param = base + 0u;
    uint param_1 = base + 4u;
    uint param_2 = base + 8u;
    uint param_3 = base + 12u;
    uint param_4 = base + 1u;
    uint param_5 = base + 5u;
    uint param_6 = base + 9u;
    uint param_7 = base + 13u;
    uint param_8 = base + 2u;
    uint param_9 = base + 6u;
    uint param_10 = base + 10u;
    uint param_11 = base + 14u;
    uint param_12 = base + 3u;
    uint param_13 = base + 7u;
    uint param_14 = base + 11u;
    uint param_15 = base + 15u;
    return float4x4(float4(sourceValue(param, _95), sourceValue(param_1, _95), sourceValue(param_2, _95), sourceValue(param_3, _95)), float4(sourceValue(param_4, _95), sourceValue(param_5, _95), sourceValue(param_6, _95), sourceValue(param_7, _95)), float4(sourceValue(param_8, _95), sourceValue(param_9, _95), sourceValue(param_10, _95), sourceValue(param_11, _95)), float4(sourceValue(param_12, _95), sourceValue(param_13, _95), sourceValue(param_14, _95), sourceValue(param_15, _95)));
}

static inline __attribute__((always_inline))
void evaluate(device Bones& _83, device Sources& _95, device Parameters& _382, thread float& inX, thread float& inY, thread float& inDepth, thread float& outDeformX, thread float& outDeformY)
{
    float x = inX;
    float y = inY;
    float z = inDepth;
    uint influenceCount = 0u;
    int lockedTerminal = -1;
    float lockedScore = 0.0;
    float lockedDistance = 0.0;
    uint lockedBoneIndex = 0u;
    uint lockedSourceIndex = 0u;
    float3 lockedRest = float3(0.0);
    spvUnsafeArray<float, 16> scores;
    spvUnsafeArray<float, 16> distances;
    spvUnsafeArray<uint, 16> boneIndices;
    spvUnsafeArray<uint, 16> sourceIndices;
    spvUnsafeArray<float3, 16> rests;
    for (uint s = 0u; s < _382.sourceCount; s++)
    {
        uint param = s;
        uint param_1 = 0u;
        uint boneIndex = uint(sourceValue(param, param_1, _95));
        if (boneIndex >= _382.boneCount)
        {
            continue;
        }
        uint param_2 = s;
        uint param_3 = 4u;
        float weight = sourceValue(param_2, param_3, _95);
        uint param_4 = s;
        uint param_5 = 5u;
        float depthScale = sourceValue(param_4, param_5, _95);
        uint param_6 = s;
        uint param_7 = 6u;
        float depthOffset = sourceValue(param_6, param_7, _95);
        uint param_8 = s;
        uint param_9 = 7u;
        float multiplier = sourceValue(param_8, param_9, _95);
        float score = weight * multiplier;
        if (!(score > 0.0))
        {
            continue;
        }
        float sourceDepth = (z * depthScale) + depthOffset;
        float3 sourceRestLocal = float3(x, y, sourceDepth);
        float3 sourceRest = (float4(sourceRestLocal, 1.0) * _382.targetToRoot).xyz;
        uint param_10 = boneIndex;
        float3 restHead = boneRestHead(param_10, _83);
        uint param_11 = boneIndex;
        float3 restTail = boneRestTail(param_11, _83);
        uint param_12 = boneIndex;
        float restLength = fast::max(boneRestLength(param_12, _83), 9.9999997473787516355514526367188e-05);
        float distanceSq = 0.0;
        float terminalDistanceSq = 3.4028230607370965250836333559001e+38;
        float terminalProjection = 0.0;
        float radius = fast::max((restLength * 0.85000002384185791015625) * _382.radiusScale, _382.influenceRadiusFloor);
        float radiusSq = radius * radius;
        if (_382.sourceCount > 1u)
        {
            float3 param_13 = sourceRest;
            float3 param_14 = restHead;
            float3 param_15 = restTail;
            distanceSq = distanceSqPointSegment(param_13, param_14, param_15);
            uint param_16 = s;
            uint param_17 = 3u;
            if (uint(sourceValue(param_16, param_17, _95)) == 1u)
            {
                float _distance = sqrt(distanceSq);
                score *= fast::max(0.0, 1.0 - (_distance / radius));
            }
            else
            {
                score *= exp((-distanceSq) / radiusSq);
            }
            uint param_18 = s;
            uint param_19 = 1u;
            bool _681 = uint(sourceValue(param_18, param_19, _95)) != 0u;
            bool _688;
            if (_681)
            {
                uint param_20 = boneIndex;
                _688 = boneParentIndex(param_20, _83) >= 0.0;
            }
            else
            {
                _688 = _681;
            }
            if (_688)
            {
                uint param_21 = boneIndex;
                uint parentIndex = uint(boneParentIndex(param_21, _83));
                if (parentIndex < _382.boneCount)
                {
                    uint param_22 = parentIndex;
                    float3 parentHead = boneRestHead(param_22, _83);
                    float3 param_23 = sourceRest;
                    float3 param_24 = parentHead;
                    float3 param_25 = restHead;
                    terminalProjection = pointSegmentProjection(param_23, param_24, param_25);
                    float3 param_26 = sourceRest;
                    float3 param_27 = parentHead;
                    float3 param_28 = restHead;
                    terminalDistanceSq = distanceSqPointSegment(param_26, param_27, param_28);
                }
            }
        }
        if (!(score > 0.0))
        {
            continue;
        }
        bool _728 = _382.sourceCount > 1u;
        bool _737;
        if (_728)
        {
            uint param_29 = s;
            uint param_30 = 1u;
            _737 = uint(sourceValue(param_29, param_30, _95)) != 0u;
        }
        else
        {
            _737 = _728;
        }
        bool _746;
        if (_737)
        {
            uint param_31 = s;
            uint param_32 = 2u;
            _746 = uint(sourceValue(param_31, param_32, _95)) != 0u;
        }
        else
        {
            _746 = _737;
        }
        if ((_746 && (terminalProjection > 1.0)) && (terminalDistanceSq <= radiusSq))
        {
            bool _761 = (lockedTerminal < 0) || (score > lockedScore);
            bool _772;
            if (!_761)
            {
                _772 = (score == lockedScore) && (terminalDistanceSq < lockedDistance);
            }
            else
            {
                _772 = _761;
            }
            if (_772)
            {
                lockedTerminal = int(s);
                lockedScore = score;
                lockedDistance = terminalDistanceSq;
                lockedBoneIndex = boneIndex;
                lockedSourceIndex = s;
                lockedRest = sourceRest;
            }
            continue;
        }
        spvUnsafeArray<float, 16> param_33 = scores;
        spvUnsafeArray<float, 16> param_34 = distances;
        spvUnsafeArray<uint, 16> param_35 = boneIndices;
        spvUnsafeArray<uint, 16> param_36 = sourceIndices;
        spvUnsafeArray<float3, 16> param_37 = rests;
        uint param_38 = influenceCount;
        float param_39 = score;
        float param_40 = distanceSq;
        uint param_41 = boneIndex;
        uint param_42 = s;
        float3 param_43 = sourceRest;
        insertInfluence(param_33, param_34, param_35, param_36, param_37, param_38, param_39, param_40, param_41, param_42, param_43, _382);
        scores = param_33;
        distances = param_34;
        boneIndices = param_35;
        sourceIndices = param_36;
        rests = param_37;
        influenceCount = param_38;
    }
    if (lockedTerminal >= 0)
    {
        influenceCount = 1u;
        scores[0] = 1.0;
        boneIndices[0] = lockedBoneIndex;
        sourceIndices[0] = lockedSourceIndex;
        rests[0] = lockedRest;
    }
    if (influenceCount == 0u)
    {
        outDeformX = 0.0;
        outDeformY = 0.0;
        return;
    }
    float total = 0.0;
    for (uint j = 0u; j < influenceCount; j++)
    {
        total += scores[j];
    }
    if (!(total > 9.9999999392252902907785028219223e-09))
    {
        total = 1.0;
        influenceCount = 1u;
        scores[0] = 1.0;
    }
    float3 rest = float3(0.0);
    float3 deformed = float3(0.0);
    float3 deformedPoint;
    for (uint j_1 = 0u; j_1 < influenceCount; j_1++)
    {
        uint boneIndex_1 = boneIndices[j_1];
        uint sourceIndex = sourceIndices[j_1];
        float w = scores[j_1] / total;
        rest += (rests[j_1] * w);
        uint param_44 = boneIndex_1;
        float4x4 fullSkin = boneSkinMatrix(param_44, _83);
        uint param_45 = sourceIndex;
        uint param_46 = 10u;
        float rotationPivotXShift = sourceValue(param_45, param_46, _95);
        uint param_47 = sourceIndex;
        uint param_48 = 11u;
        float poseYaw = sourceValue(param_47, param_48, _95);
        uint param_49 = sourceIndex;
        uint param_50 = 8u;
        float sinRotation = sourceValue(param_49, param_50, _95);
        uint param_51 = sourceIndex;
        uint param_52 = 9u;
        float cosRotation = sourceValue(param_51, param_52, _95);
        bool _921 = (poseYaw == 0.0) || (rotationPivotXShift == 0.0);
        bool _930;
        if (!_921)
        {
            _930 = (sinRotation == 0.0) && (cosRotation == 1.0);
        }
        else
        {
            _930 = _921;
        }
        if (_930)
        {
            deformedPoint = (fullSkin * float4(rests[j_1], 1.0)).xyz;
        }
        else
        {
            float3 rotationRest = rests[j_1] + float3(rotationPivotXShift, 0.0, 0.0);
            uint param_53 = sourceIndex;
            float4x4 noYawSkin = sourceNoYawSkinMatrix(param_53, _95);
            float3 baseDeformed = (noYawSkin * float4(rests[j_1], 1.0)).xyz;
            float3 rotationWithYaw = (fullSkin * float4(rotationRest, 1.0)).xyz;
            float3 rotationWithoutYaw = (noYawSkin * float4(rotationRest, 1.0)).xyz;
            deformedPoint = (baseDeformed + rotationWithYaw) - rotationWithoutYaw;
        }
        deformed += (deformedPoint * w);
    }
    float3 restLocal = (float4(rest, 1.0) * _382.rootToTarget).xyz;
    float3 local = (float4(deformed, 1.0) * _382.rootToTarget).xyz;
    outDeformX = local.x - restLocal.x;
    outDeformY = local.y - restLocal.y;
}

kernel void main0(constant Dispatch& dispatch [[buffer(6)]], device Positions& _1049 [[buffer(0)]], device Depths& _1062 [[buffer(1)]], device Bones& _83 [[buffer(2)]], device Sources& _95 [[buffer(3)]], device Parameters& _382 [[buffer(4)]], device Output& _1070 [[buffer(5)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    uint i = gl_GlobalInvocationID.x;
    float inX;
    float inY;
    float inDepth;
    float outDeformX;
    float outDeformY;
    if (i >= dispatch.count)
    {
        return;
    }
    inX = _1049.positions[i];
    inY = _1049.positions[dispatch.count + i];
    inDepth = _1062.depths[i];
    evaluate(_83, _95, _382, inX, inY, inDepth, outDeformX, outDeformY);
    _1070.result[i] = outDeformX;
    _1070.result[dispatch.count + i] = outDeformY;
}
