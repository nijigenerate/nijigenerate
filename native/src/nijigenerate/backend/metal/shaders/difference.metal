#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Parameters
{
    int2 sourceSize;
    int2 targetSize;
    int2 origin;
    int stage;
    int sourceHeight;
};

struct Input
{
    float4 inputData[1];
};

struct Output
{
    float4 outputData[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(8u, 8u, 1u);

static inline __attribute__((always_inline))
float4 fetchValue(thread int2& p, constant Parameters& params, device Input& _42, texture2d<float> sourceImage, sampler sourceImageSmplr)
{
    if (any(p >= params.sourceSize))
    {
        return float4(0.0);
    }
    if (params.stage != 0)
    {
        return _42.inputData[(p.y * params.sourceSize.x) + p.x];
    }
    p += params.origin;
    p.y = (params.sourceHeight - 1) - p.y;
    return float4(fast::clamp(sourceImage.read(uint2(p), 0).xyz, float3(0.0), float3(1.0)), 1.0);
}

kernel void main0(constant Parameters& params [[buffer(3)]], device Input& _42 [[buffer(1)]], device Output& _156 [[buffer(2)]], texture2d<float> sourceImage [[texture(0)]], sampler sourceImageSmplr [[sampler(0)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    int2 p = int2(gl_GlobalInvocationID.xy);
    if (any(p >= params.targetSize))
    {
        return;
    }
    float4 value;
    if (all(params.sourceSize == params.targetSize))
    {
        int2 param = p;
        float4 _121 = fetchValue(param, params, _42, sourceImage, sourceImageSmplr);
        value = _121;
    }
    else
    {
        int2 at = p * int2(2);
        value = float4(0.0);
        int2 param_1 = at;
        float4 _129 = fetchValue(param_1, params, _42, sourceImage, sourceImageSmplr);
        value += _129;
        int2 param_2 = at + int2(1, 0);
        float4 _136 = fetchValue(param_2, params, _42, sourceImage, sourceImageSmplr);
        value += _136;
        int2 param_3 = at + int2(0, 1);
        float4 _143 = fetchValue(param_3, params, _42, sourceImage, sourceImageSmplr);
        value += _143;
        int2 param_4 = at + int2(1);
        float4 _150 = fetchValue(param_4, params, _42, sourceImage, sourceImageSmplr);
        value += _150;
    }
    _156.outputData[(p.y * params.targetSize.x) + p.x] = value;
}
