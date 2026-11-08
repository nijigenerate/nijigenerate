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

struct Layers
{
    float layers[1];
};

struct Bindings
{
    float bindings[1];
};

struct Depths
{
    uint depthData[1];
};

struct Coverage
{
    uint coverageData[1];
};

struct Dispatch
{
    uint count;
};

struct Positions
{
    float positions[1];
};

struct Output
{
    float result[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(64u, 1u, 1u);

static inline __attribute__((always_inline))
float layerValue(thread const uint& index, device Layers& _87)
{
    return _87.layers[index];
}

static inline __attribute__((always_inline))
float bindingValue(thread const uint& index, device Bindings& _98)
{
    return _98.bindings[index];
}

static inline __attribute__((always_inline))
int convolutionRadius(thread const uint& convolution, thread const int& customRadius)
{
    if (convolution == 0u)
    {
        return 0;
    }
    if ((convolution == 2u) || (convolution == 4u))
    {
        return 2;
    }
    if (convolution >= 8u)
    {
        return max(1, customRadius);
    }
    return 1;
}

static inline __attribute__((always_inline))
float depthPixelValue(thread const uint& index, device Depths& _107)
{
    return float(_107.depthData[index]);
}

static inline __attribute__((always_inline))
float coveragePixelValue(thread const uint& index, device Coverage& _118)
{
    return float(_118.coverageData[index]);
}

static inline __attribute__((always_inline))
float depth01(thread const uint& base, thread const uint& channel, thread const bool& invert, device Depths& _107)
{
    uint param = base + 0u;
    float r = depthPixelValue(param, _107);
    uint param_1 = base + 1u;
    float g = depthPixelValue(param_1, _107);
    uint param_2 = base + 2u;
    float b = depthPixelValue(param_2, _107);
    float value = 0.0;
    if (channel == 1u)
    {
        value = r / 255.0;
    }
    else
    {
        if (channel == 2u)
        {
            value = g / 255.0;
        }
        else
        {
            if (channel == 3u)
            {
                value = b / 255.0;
            }
            else
            {
                if (channel == 4u)
                {
                    value = (((0.2125999927520751953125 * r) + (0.715200006961822509765625 * g)) + (0.072200000286102294921875 * b)) / 255.0;
                }
                else
                {
                    value = ((r + g) + b) / 765.0;
                }
            }
        }
    }
    float _194;
    if (invert)
    {
        _194 = 1.0 - value;
    }
    else
    {
        _194 = value;
    }
    return _194;
}

static inline __attribute__((always_inline))
bool samplePixel(thread const int& layerX, thread const int& layerY, thread const uint& width, thread const uint& height, thread const uint& channel, thread const bool& invert, thread const bool& hasCoverage, thread const bool& useCoverage, thread const float& threshold, thread float& sampledDepth, thread float& sampledWeight, device Layers& _87, device Depths& _107, device Coverage& _118)
{
    sampledDepth = 0.0;
    sampledWeight = 0.0;
    bool _367 = (layerX < 0) || (layerY < 0);
    bool _375;
    if (!_367)
    {
        _375 = layerX >= int(width);
    }
    else
    {
        _375 = _367;
    }
    bool _383;
    if (!_375)
    {
        _383 = layerY >= int(height);
    }
    else
    {
        _383 = _375;
    }
    if (_383)
    {
        return false;
    }
    uint pixel = (uint(layerY) * width) + uint(layerX);
    uint base = pixel * 4u;
    uint param = base + 3u;
    uint param_1 = 9u;
    float alpha = (depthPixelValue(param, _107) / 255.0) * fast::clamp(layerValue(param_1, _87), 0.0, 1.0);
    if (useCoverage && hasCoverage)
    {
        uint param_2 = base + 3u;
        float coverageAlpha = coveragePixelValue(param_2, _118) / 255.0;
        if (coverageAlpha <= 0.5)
        {
            return false;
        }
        alpha *= coverageAlpha;
    }
    if (!(alpha > threshold))
    {
        return false;
    }
    uint param_3 = 16u;
    uint param_4 = 17u;
    uint param_5 = base;
    uint param_6 = channel;
    bool param_7 = invert;
    float rawDepth = mix(layerValue(param_3, _87), layerValue(param_4, _87), depth01(param_5, param_6, param_7, _107));
    uint param_8 = 22u;
    rawDepth *= layerValue(param_8, _87);
    sampledDepth = rawDepth;
    sampledWeight = alpha;
    return true;
}

static inline __attribute__((always_inline))
float applyZTransform(thread const float& sampledDepth, device Layers& _87)
{
    uint param = 15u;
    uint param_1 = 14u;
    return (sampledDepth * layerValue(param, _87)) + layerValue(param_1, _87);
}

static inline __attribute__((always_inline))
bool convolutionUsesWeighted(thread const uint& convolution)
{
    return (((((convolution == 1u) || (convolution == 2u)) || (convolution == 3u)) || (convolution == 4u)) || (convolution == 8u)) || (convolution == 9u);
}

static inline __attribute__((always_inline))
float gaussianCustomWeight(thread const int& radius, thread const int& dx, thread const int& dy)
{
    float sigma = fast::max(1.0, float(radius) / 2.0);
    float distance2 = float((dx * dx) + (dy * dy));
    return exp((-distance2) / ((2.0 * sigma) * sigma));
}

static inline __attribute__((always_inline))
float kernelWeight(thread const uint& convolution, thread const int& customRadius, thread const int& dx, thread const int& dy)
{
    if (convolution == 3u)
    {
        return float((2 - abs(dx)) * (2 - abs(dy)));
    }
    if (convolution == 4u)
    {
        int _276;
        if (abs(dx) == 0)
        {
            _276 = 6;
        }
        else
        {
            _276 = (abs(dx) == 1) ? 4 : 1;
        }
        int wx = _276;
        int _291;
        if (abs(dy) == 0)
        {
            _291 = 6;
        }
        else
        {
            _291 = (abs(dy) == 1) ? 4 : 1;
        }
        int wy = _291;
        return float(wx * wy);
    }
    if (convolution == 9u)
    {
        int param = max(1, customRadius);
        int param_1 = dx;
        int param_2 = dy;
        return gaussianCustomWeight(param, param_1, param_2);
    }
    return 1.0;
}

static inline __attribute__((always_inline))
bool convolutionUsesFrontmost(thread const uint& convolution)
{
    return (convolution == 6u) || (convolution == 11u);
}

static inline __attribute__((always_inline))
bool convolutionUsesBackmost(thread const uint& convolution)
{
    return (convolution == 7u) || (convolution == 12u);
}

static inline __attribute__((always_inline))
bool convolutionUsesMedian(thread const uint& convolution)
{
    return convolution == 5u;
}

static inline __attribute__((always_inline))
void evaluate(device Layers& _87, device Bindings& _98, device Depths& _107, device Coverage& _118, thread float& inDocumentX, thread float& inDocumentY, thread float& outValid, thread float& outDepth)
{
    uint param = 4u;
    uint flags = uint(round(layerValue(param, _87)));
    bool invert = (flags & 1u) != 0u;
    bool hasCoverage = (flags & 2u) != 0u;
    uint param_1 = 2u;
    uint bindingFlags = uint(round(bindingValue(param_1, _98)));
    bool useCoverage = (bindingFlags & 1u) != 0u;
    uint param_2 = 0u;
    uint width = uint(round(layerValue(param_2, _87)));
    uint param_3 = 1u;
    uint height = uint(round(layerValue(param_3, _87)));
    uint param_4 = 12u;
    float scaleX = layerValue(param_4, _87);
    uint param_5 = 13u;
    float scaleY = layerValue(param_5, _87);
    if (scaleX == 0.0)
    {
        scaleX = 1.0;
    }
    if (scaleY == 0.0)
    {
        scaleY = 1.0;
    }
    uint param_6 = 5u;
    uint param_7 = 10u;
    float layerXf = ((inDocumentX - layerValue(param_6, _87)) - layerValue(param_7, _87)) / scaleX;
    uint param_8 = 6u;
    uint param_9 = 11u;
    float layerYf = ((inDocumentY - layerValue(param_8, _87)) - layerValue(param_9, _87)) / scaleY;
    int layerX = int(round(layerXf));
    int layerY = int(round(layerYf));
    uint param_10 = 18u;
    uint param_11 = 3u;
    float threshold = fast::max(layerValue(param_10, _87), bindingValue(param_11, _98));
    uint param_12 = 19u;
    uint channel = uint(round(layerValue(param_12, _87)));
    uint param_13 = 20u;
    uint convolution = uint(round(layerValue(param_13, _87)));
    uint param_14 = 21u;
    int customRadius = int(round(layerValue(param_14, _87)));
    uint param_15 = convolution;
    int param_16 = customRadius;
    int radius = convolutionRadius(param_15, param_16);
    float sampleDepth = 0.0;
    float sampleWeight = 0.0;
    if (convolution == 0u)
    {
        int param_17 = layerX;
        int param_18 = layerY;
        uint param_19 = width;
        uint param_20 = height;
        uint param_21 = channel;
        bool param_22 = invert;
        bool param_23 = hasCoverage;
        bool param_24 = useCoverage;
        float param_25 = threshold;
        float param_26;
        float param_27;
        bool _608 = samplePixel(param_17, param_18, param_19, param_20, param_21, param_22, param_23, param_24, param_25, param_26, param_27, _87, _107, _118);
        sampleDepth = param_26;
        sampleWeight = param_27;
        if (!_608)
        {
            outValid = 0.0;
            outDepth = 0.0;
            return;
        }
        outValid = 1.0;
        float param_28 = sampleDepth;
        outDepth = applyZTransform(param_28, _87);
        return;
    }
    bool _any = false;
    float weightedTotal = 0.0;
    float weightTotal = 0.0;
    float extremeDepth = 0.0;
    int medianCount = 0;
    int _628 = -radius;
    float param_38;
    float param_39;
    bool _730;
    spvUnsafeArray<float, 9> medianValues;
    for (int dy = _628; dy <= radius; dy++)
    {
        int _639 = -radius;
        for (int dx = _639; dx <= radius; dx++)
        {
            int param_29 = layerX + dx;
            int param_30 = layerY + dy;
            uint param_31 = width;
            uint param_32 = height;
            uint param_33 = channel;
            bool param_34 = invert;
            bool param_35 = hasCoverage;
            bool param_36 = useCoverage;
            float param_37 = threshold;
            bool _672 = samplePixel(param_29, param_30, param_31, param_32, param_33, param_34, param_35, param_36, param_37, param_38, param_39, _87, _107, _118);
            sampleDepth = param_38;
            sampleWeight = param_39;
            if (!_672)
            {
                continue;
            }
            uint param_40 = convolution;
            if (convolutionUsesWeighted(param_40))
            {
                uint param_41 = convolution;
                int param_42 = customRadius;
                int param_43 = dx;
                int param_44 = dy;
                float weight = sampleWeight * kernelWeight(param_41, param_42, param_43, param_44);
                if (weight <= 0.0)
                {
                    continue;
                }
                weightedTotal += (sampleDepth * weight);
                weightTotal += weight;
                _any = true;
            }
            else
            {
                uint param_45 = convolution;
                bool _712 = convolutionUsesFrontmost(param_45);
                bool _719;
                if (!_712)
                {
                    uint param_46 = convolution;
                    _719 = convolutionUsesBackmost(param_46);
                }
                else
                {
                    _719 = _712;
                }
                if (_719)
                {
                    bool _723 = !_any;
                    bool _741;
                    if (!_723)
                    {
                        uint param_47 = convolution;
                        if (convolutionUsesFrontmost(param_47))
                        {
                            _730 = sampleDepth > extremeDepth;
                        }
                        else
                        {
                            _730 = sampleDepth < extremeDepth;
                        }
                        _741 = _730;
                    }
                    else
                    {
                        _741 = _723;
                    }
                    if (_741)
                    {
                        extremeDepth = sampleDepth;
                    }
                    _any = true;
                }
                else
                {
                    uint param_48 = convolution;
                    if (convolutionUsesMedian(param_48))
                    {
                        if (medianCount < 9)
                        {
                            medianValues[medianCount] = sampleDepth;
                            medianCount++;
                            _any = true;
                        }
                    }
                }
            }
        }
    }
    if (!_any)
    {
        outValid = 0.0;
        outDepth = 0.0;
        return;
    }
    uint param_49 = convolution;
    if (convolutionUsesMedian(param_49))
    {
        for (int a = 0; a < 9; a++)
        {
            if (a >= medianCount)
            {
                break;
            }
            int _794 = a + 1;
            for (int b = _794; b < 9; b++)
            {
                if (b >= medianCount)
                {
                    break;
                }
                if (medianValues[b] < medianValues[a])
                {
                    float swapValue = medianValues[a];
                    medianValues[a] = medianValues[b];
                    medianValues[b] = swapValue;
                }
            }
        }
        outValid = 1.0;
        float param_50 = medianValues[medianCount / 2];
        outDepth = applyZTransform(param_50, _87);
        return;
    }
    outValid = 1.0;
    uint param_51 = convolution;
    float _843;
    if (convolutionUsesWeighted(param_51))
    {
        _843 = weightedTotal / weightTotal;
    }
    else
    {
        _843 = extremeDepth;
    }
    float param_52 = _843;
    outDepth = applyZTransform(param_52, _87);
}

kernel void main0(constant Dispatch& dispatch [[buffer(6)]], device Positions& _875 [[buffer(0)]], device Layers& _87 [[buffer(1)]], device Bindings& _98 [[buffer(2)]], device Depths& _107 [[buffer(3)]], device Coverage& _118 [[buffer(4)]], device Output& _889 [[buffer(5)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    uint i = gl_GlobalInvocationID.x;
    float inDocumentX;
    float inDocumentY;
    float outValid;
    float outDepth;
    if (i >= dispatch.count)
    {
        return;
    }
    inDocumentX = _875.positions[i * 2u];
    inDocumentY = _875.positions[(i * 2u) + 1u];
    evaluate(_87, _98, _107, _118, inDocumentX, inDocumentY, outValid, outDepth);
    _889.result[i] = outValid;
    _889.result[dispatch.count + i] = outDepth;
}
