module nijigenerate.viewport.depth.draw.gpu;

import nijigenerate.viewport.depth.common.targetview;
import nijigenerate.io.depthimage : DepthImageChannel, DepthImageConvolution;
import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.composer : DepthDrawComposeResult, DepthDrawLayerComposeStats;
import nijigenerate.viewport.depth.draw.coordinate;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive;
import std.algorithm : min, max, sort;
import std.array : join;
import std.exception : enforce;
import std.math : isFinite;

version (InDoesRender) {
import bindbc.opengl;
import std.string : toStringz;
}

enum DepthDrawGpuLayerStride = 24u;
enum DepthDrawGpuBindingStride = 8u;
enum DepthDrawGpuDocumentPositionStride = 2u;
enum DepthDrawGpuRgbaPixelStride = 4u;
enum DepthDrawGpuMaxVertices = 1_000_000u;
enum DepthDrawGpuMaxLayers = 1024u;
enum DepthDrawGpuMaxCustomRadius = 64;

enum DepthDrawGpuLayerField {
    Width = 0,
    Height = 1,
    DepthPixelOffset = 2,
    NormalCoverageOffset = 3,
    Flags = 4,
    BoundsLeft = 5,
    BoundsTop = 6,
    BoundsWidth = 7,
    BoundsHeight = 8,
    Opacity = 9,
    XyOffsetX = 10,
    XyOffsetY = 11,
    XyScaleX = 12,
    XyScaleY = 13,
    ZOffset = 14,
    ZScale = 15,
    BackDepth = 16,
    FrontDepth = 17,
    AlphaThreshold = 18,
    Channel = 19,
    Convolution = 20,
    CustomRadius = 21,
    SampleDepthScale = 22,
}

enum DepthDrawGpuBindingField {
    LayerIndex = 0,
    MergePolicy = 1,
    Flags = 2,
    CoverageThreshold = 3,
}

struct DepthDrawGpuLayerPacket {
    string layerId;
    uint width;
    uint height;
    uint depthPixelOffset;
    uint normalCoverageOffset;
    uint flags;
    float boundsLeft;
    float boundsTop;
    float boundsWidth;
    float boundsHeight;
    float opacity;
    float xyOffsetX;
    float xyOffsetY;
    float xyScaleX;
    float xyScaleY;
    float zOffset;
    float zScale;
    float backDepth;
    float frontDepth;
    float sampleDepthScale;
    float alphaThreshold;
    int channel;
    int convolution;
    int customRadius;
}

struct DepthDrawGpuBindingPacket {
    string layerId;
    uint layerIndex;
    uint mergePolicy;
    uint flags;
    float coverageThreshold;
}

struct DepthDrawGpuComposePacket {
    ulong targetGridUuid;
    int documentWidth;
    int documentHeight;
    vec2[] vertices;
    vec2[] documentPositions;
    float[] baseDepths;
    DepthDrawGpuLayerPacket[] layers;
    DepthDrawGpuBindingPacket[] bindings;
    ubyte[] depthPixels;
    ubyte[] normalCoveragePixels;
}

struct DepthDrawGpuLayerSamplePacket {
    ulong targetGridUuid;
    uint layerIndex;
    DepthDrawGpuLayerPacket layer;
    DepthDrawGpuBindingPacket binding;
    vec2[] documentPositions;
    ubyte[] depthPixels;
    ubyte[] normalCoveragePixels;
}

struct DepthDrawGpuLayerSampleUpload {
    float[] documentPositions;
    float[] layer;
    float[] binding;
    float[] depthPixels;
    float[] normalCoveragePixels;
}

struct DepthDrawGpuLayerReadback {
    uint layerIndex;
    ubyte[] validSamples;
    float[] sampleDepths;
}

struct DepthDrawGpuComposeReadback {
    ulong targetGridUuid;
    float[] depths;
    int[] winningLayerIndices;
    DepthDrawGpuLayerReadback[] layers;
}

DepthDrawGpuLayerSamplePacket ngBuildDepthDrawGpuLayerSamplePacket(
    ref const(DepthDrawGpuComposePacket) packet,
    size_t layerIndex
) {
    validateDepthDrawGpuPacket(packet);
    enforce(layerIndex < packet.layers.length, "DepthDraw GPU layer sample index is out of bounds");

    auto layer = packet.layers[layerIndex];
    auto binding = packet.bindings[layerIndex];
    auto pixelCount = cast(size_t)layer.width * cast(size_t)layer.height * 4;
    DepthDrawGpuLayerSamplePacket result;
    result.targetGridUuid = packet.targetGridUuid;
    result.layerIndex = cast(uint)layerIndex;
    result.layer = layer;
    result.layer.depthPixelOffset = 0;
    result.layer.normalCoverageOffset = layer.normalCoverageOffset == uint.max ? uint.max : 0;
    result.binding = binding;
    result.binding.layerIndex = 0;
    result.documentPositions = packet.documentPositions.dup;
    result.depthPixels = packet.depthPixels[
        cast(size_t)layer.depthPixelOffset .. cast(size_t)layer.depthPixelOffset + pixelCount
    ].dup;
    if (layer.normalCoverageOffset != uint.max) {
        result.normalCoveragePixels = packet.normalCoveragePixels[
            cast(size_t)layer.normalCoverageOffset .. cast(size_t)layer.normalCoverageOffset + pixelCount
        ].dup;
    }
    return result;
}

void ngValidateDepthDrawGpuLayerSamplePacket(ref const(DepthDrawGpuLayerSamplePacket) packet) {
    validateDepthDrawGpuLayerSamplePacket(packet);
}

DepthDrawGpuLayerSampleUpload ngBuildDepthDrawGpuLayerSampleUpload(
    ref const(DepthDrawGpuLayerSamplePacket) packet
) {
    validateDepthDrawGpuLayerSamplePacket(packet);
    DepthDrawGpuLayerSampleUpload upload;
    upload.documentPositions = ngFlattenDepthDrawGpuDocumentPositions(packet.documentPositions);
    upload.layer = ngFlattenDepthDrawGpuLayers([packet.layer]);
    upload.binding = ngFlattenDepthDrawGpuBindings([packet.binding]);
    upload.depthPixels = ngFlattenDepthDrawGpuRgbaBytes(packet.depthPixels);
    if (packet.normalCoveragePixels.length > 0) {
        upload.normalCoveragePixels = ngFlattenDepthDrawGpuRgbaBytes(packet.normalCoveragePixels);
    }
    return upload;
}

float[] ngFlattenDepthDrawGpuDocumentPositions(const(vec2)[] documentPositions) {
    float[] values;
    values.length = documentPositions.length * DepthDrawGpuDocumentPositionStride;
    foreach (i, point; documentPositions) {
        auto base = i * DepthDrawGpuDocumentPositionStride;
        values[base] = point.x;
        values[base + 1] = point.y;
    }
    return values;
}

float[] ngFlattenDepthDrawGpuRgbaBytes(const(ubyte)[] pixels) {
    float[] values;
    values.length = pixels.length;
    foreach (i, value; pixels) {
        values[i] = cast(float)value;
    }
    return values;
}

struct DepthDrawGpuDispatchPollResult {
    bool ready;
    DepthDrawGpuComposeReadback readback;
}

struct DepthDrawGpuLayerSamplePollResult {
    bool ready;
    DepthDrawGpuLayerReadback readback;
}

struct DepthDrawGpuTargetComposeJob {
    uint jobId;
    DepthDrawGpuComposePacket packet;
}

struct DepthDrawGpuTargetComposePollResult {
    bool ready;
    DepthDrawGpuComposeReadback readback;
    DepthDrawComposeResult result;
}

alias DepthDrawGpuSupportHook = bool function();
alias DepthDrawGpuSubmitHook = bool function(ref DepthDrawGpuComposePacket packet, out uint jobId, out string error);
alias DepthDrawGpuPollHook = bool function(uint jobId, out DepthDrawGpuDispatchPollResult result, out string error);

private DepthDrawGpuSupportHook supportHook;
private DepthDrawGpuSubmitHook submitHook;
private DepthDrawGpuPollHook pollHook;
private DepthDrawGpuComposePacket[uint] pendingPackets;

version (InDoesRender) {
private __gshared GLuint depthDrawLayerSampleProgram;
private __gshared GLint uDepthDrawLayer = -1;
private __gshared GLint uDepthDrawBinding = -1;
private __gshared GLint uDepthDrawDepthPixels = -1;
private __gshared GLint uDepthDrawNormalCoveragePixels = -1;
private __gshared uint nextLayerSampleJobId = 1;

private struct PendingDepthDrawLayerSampleJob {
    uint id;
    uint layerIndex;
    size_t count;
    GLuint documentPositionsBuffer;
    GLuint layerBuffer;
    GLuint bindingBuffer;
    GLuint depthPixelsBuffer;
    GLuint normalCoveragePixelsBuffer;
    GLuint outputBuffer;
    GLuint layerTexture;
    GLuint bindingTexture;
    GLuint depthPixelsTexture;
    GLuint normalCoveragePixelsTexture;
    GLuint vertexArray;
    GLsync fence;
}

private struct PendingDepthDrawComposeJob {
    uint id;
    uint[] layerJobIds;
    DepthDrawGpuLayerReadback[] layerReadbacks;
    bool[] layerReady;
}

private __gshared PendingDepthDrawLayerSampleJob[] pendingLayerSampleJobs;
private __gshared PendingDepthDrawComposeJob[] pendingComposeJobs;
private __gshared uint nextComposeJobId = 1;

private enum string LayerSampleVertexSource = q"GLSL
#version 330

#define LAYER_WIDTH 0u
#define LAYER_HEIGHT 1u
#define LAYER_FLAGS 4u
#define LAYER_BOUNDS_LEFT 5u
#define LAYER_BOUNDS_TOP 6u
#define LAYER_OPACITY 9u
#define LAYER_XY_OFFSET_X 10u
#define LAYER_XY_OFFSET_Y 11u
#define LAYER_XY_SCALE_X 12u
#define LAYER_XY_SCALE_Y 13u
#define LAYER_Z_OFFSET 14u
#define LAYER_Z_SCALE 15u
#define LAYER_BACK_DEPTH 16u
#define LAYER_FRONT_DEPTH 17u
#define LAYER_ALPHA_THRESHOLD 18u
#define LAYER_CHANNEL 19u
#define LAYER_CONVOLUTION 20u
#define LAYER_CUSTOM_RADIUS 21u
#define LAYER_SAMPLE_DEPTH_SCALE 22u

#define BINDING_FLAGS 2u
#define BINDING_COVERAGE_THRESHOLD 3u

layout(location = 0) in float inDocumentX;
layout(location = 1) in float inDocumentY;

out float outValid;
out float outDepth;

uniform samplerBuffer layerData;
uniform samplerBuffer bindingData;
uniform samplerBuffer depthPixels;
uniform samplerBuffer normalCoveragePixels;

float layerValue(uint index) {
    return texelFetch(layerData, int(index)).r;
}

float bindingValue(uint index) {
    return texelFetch(bindingData, int(index)).r;
}

float depthPixelValue(uint index) {
    return texelFetch(depthPixels, int(index)).r;
}

float coveragePixelValue(uint index) {
    return texelFetch(normalCoveragePixels, int(index)).r;
}

float depth01(uint base, uint channel, bool invert) {
    float r = depthPixelValue(base + 0u);
    float g = depthPixelValue(base + 1u);
    float b = depthPixelValue(base + 2u);
    float value = 0.0;
    if (channel == 1u) {
        value = r / 255.0;
    } else if (channel == 2u) {
        value = g / 255.0;
    } else if (channel == 3u) {
        value = b / 255.0;
    } else if (channel == 4u) {
        value = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    } else {
        value = (r + g + b) / (255.0 * 3.0);
    }
    return invert ? 1.0 - value : value;
}

int convolutionRadius(uint convolution, int customRadius) {
    if (convolution == 0u) return 0;
    if (convolution == 2u || convolution == 4u) return 2;
    if (convolution >= 8u) return max(1, customRadius);
    return 1;
}

float gaussianCustomWeight(int radius, int dx, int dy) {
    float sigma = max(1.0, float(radius) / 2.0);
    float distance2 = float(dx * dx + dy * dy);
    return exp(-distance2 / (2.0 * sigma * sigma));
}

float kernelWeight(uint convolution, int customRadius, int dx, int dy) {
    if (convolution == 3u) {
        return float((2 - abs(dx)) * (2 - abs(dy)));
    }
    if (convolution == 4u) {
        int wx = abs(dx) == 0 ? 6 : (abs(dx) == 1 ? 4 : 1);
        int wy = abs(dy) == 0 ? 6 : (abs(dy) == 1 ? 4 : 1);
        return float(wx * wy);
    }
    if (convolution == 9u) {
        return gaussianCustomWeight(max(1, customRadius), dx, dy);
    }
    return 1.0;
}

bool convolutionUsesWeighted(uint convolution) {
    return convolution == 1u || convolution == 2u || convolution == 3u ||
        convolution == 4u || convolution == 8u || convolution == 9u;
}

bool convolutionUsesFrontmost(uint convolution) {
    return convolution == 6u || convolution == 11u;
}

bool convolutionUsesBackmost(uint convolution) {
    return convolution == 7u || convolution == 12u;
}

bool convolutionUsesMedian(uint convolution) {
    return convolution == 5u;
}

bool samplePixel(
    int layerX,
    int layerY,
    uint width,
    uint height,
    uint channel,
    bool invert,
    bool hasCoverage,
    bool useCoverage,
    float threshold,
    out float sampledDepth,
    out float sampledWeight
) {
    sampledDepth = 0.0;
    sampledWeight = 0.0;
    if (layerX < 0 || layerY < 0 || layerX >= int(width) || layerY >= int(height)) return false;
    uint pixel = uint(layerY) * width + uint(layerX);
    uint base = pixel * 4u;
    float alpha = (depthPixelValue(base + 3u) / 255.0) * clamp(layerValue(LAYER_OPACITY), 0.0, 1.0);
    if (useCoverage && hasCoverage) {
        float coverageAlpha = coveragePixelValue(base + 3u) / 255.0;
        if (coverageAlpha <= 0.5) return false;
        alpha *= coverageAlpha;
    }
    if (!(alpha > threshold)) return false;
    float rawDepth = mix(layerValue(LAYER_BACK_DEPTH), layerValue(LAYER_FRONT_DEPTH), depth01(base, channel, invert));
    rawDepth *= layerValue(LAYER_SAMPLE_DEPTH_SCALE);
    sampledDepth = rawDepth * layerValue(LAYER_Z_SCALE) + layerValue(LAYER_Z_OFFSET);
    sampledWeight = alpha;
    return true;
}

void main() {
    uint flags = uint(round(layerValue(LAYER_FLAGS)));
    bool invert = (flags & 1u) != 0u;
    bool hasCoverage = (flags & 2u) != 0u;
    uint bindingFlags = uint(round(bindingValue(BINDING_FLAGS)));
    bool useCoverage = (bindingFlags & 1u) != 0u;

    uint width = uint(round(layerValue(LAYER_WIDTH)));
    uint height = uint(round(layerValue(LAYER_HEIGHT)));
    float scaleX = layerValue(LAYER_XY_SCALE_X);
    float scaleY = layerValue(LAYER_XY_SCALE_Y);
    if (scaleX == 0.0) scaleX = 1.0;
    if (scaleY == 0.0) scaleY = 1.0;

    float layerXf = (inDocumentX - layerValue(LAYER_BOUNDS_LEFT) - layerValue(LAYER_XY_OFFSET_X)) / scaleX;
    float layerYf = (inDocumentY - layerValue(LAYER_BOUNDS_TOP) - layerValue(LAYER_XY_OFFSET_Y)) / scaleY;
    int layerX = int(round(layerXf));
    int layerY = int(round(layerYf));
    float threshold = max(layerValue(LAYER_ALPHA_THRESHOLD), bindingValue(BINDING_COVERAGE_THRESHOLD));
    uint channel = uint(round(layerValue(LAYER_CHANNEL)));
    uint convolution = uint(round(layerValue(LAYER_CONVOLUTION)));
    int customRadius = int(round(layerValue(LAYER_CUSTOM_RADIUS)));
    int radius = convolutionRadius(convolution, customRadius);

    float sampleDepth = 0.0;
    float sampleWeight = 0.0;
    if (convolution == 0u) {
        if (!samplePixel(layerX, layerY, width, height, channel, invert, hasCoverage, useCoverage,
            threshold, sampleDepth, sampleWeight)) {
            outValid = 0.0;
            outDepth = 0.0;
            return;
        }
        outValid = 1.0;
        outDepth = sampleDepth;
        return;
    }

    bool any = false;
    float weightedTotal = 0.0;
    float weightTotal = 0.0;
    float extremeDepth = 0.0;
    float medianValues[9];
    int medianCount = 0;
    for (int dy = -64; dy <= 64; ++dy) {
        if (dy < -radius || dy > radius) continue;
        for (int dx = -64; dx <= 64; ++dx) {
            if (dx < -radius || dx > radius) continue;
            if (!samplePixel(layerX + dx, layerY + dy, width, height, channel, invert, hasCoverage,
                useCoverage, threshold, sampleDepth, sampleWeight)) {
                continue;
            }
            if (convolutionUsesWeighted(convolution)) {
                float weight = sampleWeight * kernelWeight(convolution, customRadius, dx, dy);
                if (weight <= 0.0) continue;
                weightedTotal += sampleDepth * weight;
                weightTotal += weight;
                any = true;
            } else if (convolutionUsesFrontmost(convolution) || convolutionUsesBackmost(convolution)) {
                if (!any || (convolutionUsesFrontmost(convolution) ? sampleDepth > extremeDepth : sampleDepth < extremeDepth)) {
                    extremeDepth = sampleDepth;
                }
                any = true;
            } else if (convolutionUsesMedian(convolution)) {
                if (medianCount < 9) {
                    medianValues[medianCount] = sampleDepth;
                    medianCount += 1;
                    any = true;
                }
            }
        }
    }

    if (!any) {
        outValid = 0.0;
        outDepth = 0.0;
        return;
    }
    if (convolutionUsesMedian(convolution)) {
        for (int a = 0; a < 9; ++a) {
            if (a >= medianCount) break;
            for (int b = a + 1; b < 9; ++b) {
                if (b >= medianCount) break;
                if (medianValues[b] < medianValues[a]) {
                    float swapValue = medianValues[a];
                    medianValues[a] = medianValues[b];
                    medianValues[b] = swapValue;
                }
            }
        }
        outValid = 1.0;
        outDepth = medianValues[medianCount / 2];
        return;
    }
    outValid = 1.0;
    outDepth = convolutionUsesWeighted(convolution) ? (weightedTotal / weightTotal) : extremeDepth;
}
GLSL";
}

void ngSetDepthDrawGpuTestHooks(
    DepthDrawGpuSupportHook support,
    DepthDrawGpuSubmitHook submit,
    DepthDrawGpuPollHook poll
) {
    supportHook = support;
    submitHook = submit;
    pollHook = poll;
}

void ngClearDepthDrawGpuTestHooks() {
    supportHook = null;
    submitHook = null;
    pollHook = null;
    pendingPackets = null;
    version (InDoesRender) {
        pendingComposeJobs = null;
    }
}

bool ngDepthDrawGpuSupported() {
    if (supportHook !is null) return supportHook();
    return ngDepthDrawGpuMissingRequirements().length == 0;
}

string[] ngDepthDrawGpuMissingRequirements() {
    string[] missing;
    version (InDoesRender) {
        if (glCreateShader is null) missing ~= "glCreateShader";
        if (glTransformFeedbackVaryings is null) missing ~= "glTransformFeedbackVaryings";
        if (glBeginTransformFeedback is null) missing ~= "glBeginTransformFeedback";
        if (glEndTransformFeedback is null) missing ~= "glEndTransformFeedback";
        if (glBindBufferRange is null) missing ~= "glBindBufferRange";
        if (glBindBufferBase is null) missing ~= "glBindBufferBase";
        if (glGetBufferSubData is null) missing ~= "glGetBufferSubData";
        if (glGenVertexArrays is null) missing ~= "glGenVertexArrays";
        if (glBindVertexArray is null) missing ~= "glBindVertexArray";
        if (glDeleteVertexArrays is null) missing ~= "glDeleteVertexArrays";
        if (glTexBuffer is null) missing ~= "glTexBuffer";
        if (glFenceSync is null) missing ~= "glFenceSync";
        if (glClientWaitSync is null) missing ~= "glClientWaitSync";
        if (glDeleteSync is null) missing ~= "glDeleteSync";
    } else {
        missing ~= "rendering backend";
    }
    return missing;
}

string ngDepthDrawGpuSupportDiagnostic() {
    if (ngDepthDrawGpuSupported()) return "available";
    return ngDepthDrawGpuMissingRequirements().join(", ");
}

string[] ngDepthDrawGpuLayerSampleMissingRequirements() {
    string[] missing;
    version (InDoesRender) {
        if (glCreateShader is null) missing ~= "glCreateShader";
        if (glShaderSource is null) missing ~= "glShaderSource";
        if (glCompileShader is null) missing ~= "glCompileShader";
        if (glCreateProgram is null) missing ~= "glCreateProgram";
        if (glTransformFeedbackVaryings is null) missing ~= "glTransformFeedbackVaryings";
        if (glBeginTransformFeedback is null) missing ~= "glBeginTransformFeedback";
        if (glEndTransformFeedback is null) missing ~= "glEndTransformFeedback";
        if (glBindBufferRange is null) missing ~= "glBindBufferRange";
        if (glBindBufferBase is null) missing ~= "glBindBufferBase";
        if (glGetBufferSubData is null) missing ~= "glGetBufferSubData";
        if (glGenVertexArrays is null) missing ~= "glGenVertexArrays";
        if (glBindVertexArray is null) missing ~= "glBindVertexArray";
        if (glDeleteVertexArrays is null) missing ~= "glDeleteVertexArrays";
        if (glTexBuffer is null) missing ~= "glTexBuffer";
        if (glFenceSync is null) missing ~= "glFenceSync";
        if (glClientWaitSync is null) missing ~= "glClientWaitSync";
        if (glDeleteSync is null) missing ~= "glDeleteSync";
    } else {
        missing ~= "rendering backend";
    }
    return missing;
}

bool ngDepthDrawGpuLayerSampleSupported() {
    return ngDepthDrawGpuLayerSampleMissingRequirements().length == 0;
}

bool ngDepthDrawGpuLayerSampleSupportsConvolution(int convolutionValue) {
    if (!isValidDepthDrawGpuConvolution(convolutionValue)) return false;
    auto convolution = cast(DepthImageConvolution)convolutionValue;
    switch (convolution) {
        case DepthImageConvolution.MedianCustom:
            return false;
        default:
            return true;
    }
}

string ngDepthDrawGpuLayerSampleSupportDiagnostic() {
    if (ngDepthDrawGpuLayerSampleSupported()) return "available";
    return ngDepthDrawGpuLayerSampleMissingRequirements().join(", ");
}

version (InDoesRender) {
private void checkDepthDrawGpuShader(GLuint shader, string label) {
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) return;
    GLint logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    string log;
    if (logLength > 1) {
        log.length = cast(size_t)logLength;
        glGetShaderInfoLog(shader, logLength, null, cast(GLchar*)log.ptr);
    }
    enforce(false, label ~ " compile failed: " ~ log);
}

private void checkDepthDrawGpuProgram(GLuint program) {
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) return;
    GLint logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    string log;
    if (logLength > 1) {
        log.length = cast(size_t)logLength;
        glGetProgramInfoLog(program, logLength, null, cast(GLchar*)log.ptr);
    }
    enforce(false, "DepthDraw layer sample transform feedback shader link failed: " ~ log);
}

private void ensureDepthDrawLayerSampleProgram() {
    if (depthDrawLayerSampleProgram != 0) return;
    auto shader = glCreateShader(GL_VERTEX_SHADER);
    auto src = LayerSampleVertexSource.toStringz;
    glShaderSource(shader, 1, &src, null);
    glCompileShader(shader);
    checkDepthDrawGpuShader(shader, "DepthDraw layer sample transform feedback shader");

    depthDrawLayerSampleProgram = glCreateProgram();
    glAttachShader(depthDrawLayerSampleProgram, shader);
    const(char)*[2] varyings = ["outValid".toStringz, "outDepth".toStringz];
    glTransformFeedbackVaryings(
        depthDrawLayerSampleProgram,
        cast(GLsizei)varyings.length,
        varyings.ptr,
        GL_SEPARATE_ATTRIBS);
    glLinkProgram(depthDrawLayerSampleProgram);
    glDetachShader(depthDrawLayerSampleProgram, shader);
    glDeleteShader(shader);
    checkDepthDrawGpuProgram(depthDrawLayerSampleProgram);

    uDepthDrawLayer = glGetUniformLocation(depthDrawLayerSampleProgram, "layerData");
    uDepthDrawBinding = glGetUniformLocation(depthDrawLayerSampleProgram, "bindingData");
    uDepthDrawDepthPixels = glGetUniformLocation(depthDrawLayerSampleProgram, "depthPixels");
    uDepthDrawNormalCoveragePixels = glGetUniformLocation(depthDrawLayerSampleProgram, "normalCoveragePixels");
}

private GLuint createDepthDrawGpuBuffer(GLenum target, const(void)[] bytes, GLenum usage = GL_DYNAMIC_DRAW) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, cast(GLsizeiptr)bytes.length, bytes.ptr, usage);
    return buffer;
}

private void createDepthDrawGpuTextureBuffer(ref GLuint buffer, ref GLuint texture, const(float)[] data) {
    buffer = createDepthDrawGpuBuffer(GL_TEXTURE_BUFFER, cast(const(void)[])data);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

private void deleteDepthDrawLayerSampleJobResources(ref PendingDepthDrawLayerSampleJob job) {
    if (job.fence !is null && glDeleteSync !is null) {
        glDeleteSync(job.fence);
        job.fence = null;
    }
    GLuint[6] buffers = [
        job.documentPositionsBuffer,
        job.layerBuffer,
        job.bindingBuffer,
        job.depthPixelsBuffer,
        job.normalCoveragePixelsBuffer,
        job.outputBuffer,
    ];
    bool hasBuffer;
    foreach (buffer; buffers) hasBuffer = hasBuffer || buffer != 0;
    if (hasBuffer && glDeleteBuffers !is null) glDeleteBuffers(cast(GLsizei)buffers.length, buffers.ptr);
    GLuint[4] textures = [
        job.layerTexture,
        job.bindingTexture,
        job.depthPixelsTexture,
        job.normalCoveragePixelsTexture,
    ];
    bool hasTexture;
    foreach (texture; textures) hasTexture = hasTexture || texture != 0;
    if (hasTexture && glDeleteTextures !is null) glDeleteTextures(cast(GLsizei)textures.length, textures.ptr);
    if (job.vertexArray != 0 && glDeleteVertexArrays !is null) glDeleteVertexArrays(1, &job.vertexArray);
    job = PendingDepthDrawLayerSampleJob.init;
}

private bool cancelDepthDrawGpuLayerSampleJob(uint jobId) {
    foreach (i; 0 .. pendingLayerSampleJobs.length) {
        if (pendingLayerSampleJobs[i].id != jobId) continue;
        deleteDepthDrawLayerSampleJobResources(pendingLayerSampleJobs[i]);
        pendingLayerSampleJobs = pendingLayerSampleJobs[0 .. i] ~ pendingLayerSampleJobs[i + 1 .. $];
        return true;
    }
    return false;
}

bool ngSubmitDepthDrawGpuLayerSample(
    ref DepthDrawGpuLayerSamplePacket packet,
    out uint jobId,
    out string error
) {
    jobId = 0;
    error = null;
    PendingDepthDrawLayerSampleJob job;
    bool queued;
    try {
        enforce(ngDepthDrawGpuLayerSampleSupported(),
            "DepthDraw GPU layer sampling requires OpenGL transform feedback support: " ~
            ngDepthDrawGpuLayerSampleSupportDiagnostic());
        validateDepthDrawGpuLayerSamplePacket(packet);
        enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(packet.layer.convolution),
            "DepthDraw GPU layer sampling does not support custom median convolution yet");
        ensureDepthDrawLayerSampleProgram();

        GLint previousProgram;
        GLint previousVertexArray;
        GLint previousArrayBuffer;
        GLint previousTexture0Buffer;
        GLint previousTexture1Buffer;
        GLint previousTexture2Buffer;
        GLint previousTexture3Buffer;
        GLint previousActiveTexture;
        GLboolean rasterizerDiscardWasEnabled = glIsEnabled(GL_RASTERIZER_DISCARD);
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &previousTexture0Buffer);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &previousTexture1Buffer);
        glActiveTexture(GL_TEXTURE2);
        glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &previousTexture2Buffer);
        glActiveTexture(GL_TEXTURE3);
        glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &previousTexture3Buffer);
        glActiveTexture(cast(GLenum)previousActiveTexture);
        scope(exit) {
            glBindBuffer(GL_ARRAY_BUFFER, cast(GLuint)previousArrayBuffer);
            glBindVertexArray(cast(GLuint)previousVertexArray);
            glUseProgram(cast(GLuint)previousProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture0Buffer);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture1Buffer);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture2Buffer);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture3Buffer);
            glActiveTexture(cast(GLenum)previousActiveTexture);
            if (rasterizerDiscardWasEnabled == GL_TRUE) glEnable(GL_RASTERIZER_DISCARD);
            else glDisable(GL_RASTERIZER_DISCARD);
        }

        auto upload = ngBuildDepthDrawGpuLayerSampleUpload(packet);
        job.id = nextLayerSampleJobId++;
        job.layerIndex = packet.layerIndex;
        job.count = packet.documentPositions.length;
        glGenVertexArrays(1, &job.vertexArray);
        glBindVertexArray(job.vertexArray);
        job.documentPositionsBuffer = createDepthDrawGpuBuffer(GL_ARRAY_BUFFER, cast(const(void)[])upload.documentPositions);
        createDepthDrawGpuTextureBuffer(job.layerBuffer, job.layerTexture, upload.layer);
        createDepthDrawGpuTextureBuffer(job.bindingBuffer, job.bindingTexture, upload.binding);
        createDepthDrawGpuTextureBuffer(job.depthPixelsBuffer, job.depthPixelsTexture, upload.depthPixels);
        auto coveragePixels = upload.normalCoveragePixels.length > 0 ? upload.normalCoveragePixels : [0.0f, 0.0f, 0.0f, 0.0f];
        createDepthDrawGpuTextureBuffer(job.normalCoveragePixelsBuffer, job.normalCoveragePixelsTexture, coveragePixels);

        auto count = packet.documentPositions.length;
        auto laneBytes = cast(GLsizeiptr)(count * float.sizeof);
        glGenBuffers(1, &job.outputBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, job.outputBuffer);
        glBufferData(GL_ARRAY_BUFFER, laneBytes * 2, null, GL_STREAM_READ);

        glUseProgram(depthDrawLayerSampleProgram);
        glUniform1i(uDepthDrawLayer, 0);
        glUniform1i(uDepthDrawBinding, 1);
        glUniform1i(uDepthDrawDepthPixels, 2);
        glUniform1i(uDepthDrawNormalCoveragePixels, 3);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, job.layerTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, job.bindingTexture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_BUFFER, job.depthPixelsTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_BUFFER, job.normalCoveragePixelsTexture);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, job.documentPositionsBuffer);
        glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, cast(GLsizei)(DepthDrawGpuDocumentPositionStride * float.sizeof), cast(void*)0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, job.documentPositionsBuffer);
        glVertexAttribPointer(
            1,
            1,
            GL_FLOAT,
            GL_FALSE,
            cast(GLsizei)(DepthDrawGpuDocumentPositionStride * float.sizeof),
            cast(void*)(cast(GLintptr)float.sizeof));

        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, job.outputBuffer, 0, laneBytes);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, job.outputBuffer, laneBytes, laneBytes);

        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, cast(GLsizei)count);
        glEndTransformFeedback();
        if (rasterizerDiscardWasEnabled == GL_FALSE) glDisable(GL_RASTERIZER_DISCARD);

        job.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();

        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, 0);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        foreach (textureUnit; [GL_TEXTURE3, GL_TEXTURE2, GL_TEXTURE1, GL_TEXTURE0]) {
            glActiveTexture(textureUnit);
            glBindTexture(GL_TEXTURE_BUFFER, 0);
        }

        pendingLayerSampleJobs ~= job;
        queued = true;
        jobId = job.id;
        return true;
    } catch (Exception e) {
        if (!queued) deleteDepthDrawLayerSampleJobResources(job);
        error = e.msg;
        return false;
    }
}

private bool pollDepthDrawGpuLayerSample(
    uint jobId,
    ulong timeoutNanoseconds,
    out DepthDrawGpuLayerSamplePollResult result,
    out string error
) {
    result = DepthDrawGpuLayerSamplePollResult.init;
    error = null;
    foreach (i; 0 .. pendingLayerSampleJobs.length) {
        if (pendingLayerSampleJobs[i].id != jobId) continue;
        auto flags = cast(GLbitfield)(timeoutNanoseconds > 0 ? GL_SYNC_FLUSH_COMMANDS_BIT : 0);
        auto status = glClientWaitSync(pendingLayerSampleJobs[i].fence, flags, timeoutNanoseconds);
        if (status == GL_TIMEOUT_EXPIRED) return true;
        if (status == GL_WAIT_FAILED) {
            error = "DepthDraw GPU layer sample fence wait failed";
            deleteDepthDrawLayerSampleJobResources(pendingLayerSampleJobs[i]);
            pendingLayerSampleJobs = pendingLayerSampleJobs[0 .. i] ~ pendingLayerSampleJobs[i + 1 .. $];
            return false;
        }

        float[] validFloats;
        float[] sampleDepths;
        validFloats.length = pendingLayerSampleJobs[i].count;
        sampleDepths.length = pendingLayerSampleJobs[i].count;
        auto byteCount = cast(GLsizeiptr)(pendingLayerSampleJobs[i].count * float.sizeof);
        auto depthOffset = cast(GLintptr)(pendingLayerSampleJobs[i].count * float.sizeof);
        GLint previousArrayBuffer;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, pendingLayerSampleJobs[i].outputBuffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, byteCount, validFloats.ptr);
        glGetBufferSubData(GL_ARRAY_BUFFER, depthOffset, byteCount, sampleDepths.ptr);
        glBindBuffer(GL_ARRAY_BUFFER, cast(GLuint)previousArrayBuffer);

        result.ready = true;
        result.readback.layerIndex = pendingLayerSampleJobs[i].layerIndex;
        result.readback.validSamples.length = validFloats.length;
        foreach (j, valid; validFloats) {
            result.readback.validSamples[j] = valid > 0.5f ? 1 : 0;
        }
        result.readback.sampleDepths = sampleDepths;
        deleteDepthDrawLayerSampleJobResources(pendingLayerSampleJobs[i]);
        pendingLayerSampleJobs = pendingLayerSampleJobs[0 .. i] ~ pendingLayerSampleJobs[i + 1 .. $];
        return true;
    }
    error = "DepthDraw GPU layer sample job was not found";
    return false;
}

bool ngPollDepthDrawGpuLayerSample(
    uint jobId,
    out DepthDrawGpuLayerSamplePollResult result,
    out string error
) {
    return pollDepthDrawGpuLayerSample(jobId, 0, result, error);
}

size_t ngPendingDepthDrawGpuLayerSampleJobCount() {
    return pendingLayerSampleJobs.length;
}

private bool submitDepthDrawGpuComposeBackend(
    ref DepthDrawGpuComposePacket packet,
    out uint jobId,
    out string error
) {
    jobId = 0;
    error = null;
    PendingDepthDrawComposeJob job;
    try {
        enforce(ngDepthDrawGpuLayerSampleSupported(),
            "DepthDraw GPU composition requires OpenGL layer sampling support: " ~
            ngDepthDrawGpuLayerSampleSupportDiagnostic());
        foreach (layer; packet.layers) {
            enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(layer.convolution),
                "DepthDraw GPU composition does not support custom median convolution yet");
        }
        job.id = nextComposeJobId++;
        job.layerJobIds.length = packet.layers.length;
        job.layerReadbacks.length = packet.layers.length;
        job.layerReady.length = packet.layers.length;
        foreach (layerIndex; 0 .. packet.layers.length) {
            auto samplePacket = ngBuildDepthDrawGpuLayerSamplePacket(packet, layerIndex);
            uint layerJobId;
            if (!ngSubmitDepthDrawGpuLayerSample(samplePacket, layerJobId, error)) {
                foreach (submittedJobId; job.layerJobIds) {
                    if (submittedJobId != 0) cancelDepthDrawGpuLayerSampleJob(submittedJobId);
                }
                return false;
            }
            job.layerJobIds[layerIndex] = layerJobId;
        }
        pendingComposeJobs ~= job;
        jobId = job.id;
        return true;
    } catch (Exception e) {
        foreach (submittedJobId; job.layerJobIds) {
            if (submittedJobId != 0) cancelDepthDrawGpuLayerSampleJob(submittedJobId);
        }
        error = e.msg;
        return false;
    }
}

private bool pollDepthDrawGpuComposeBackend(
    uint jobId,
    ref const(DepthDrawGpuComposePacket) packet,
    out DepthDrawGpuDispatchPollResult result,
    out string error
) {
    result = DepthDrawGpuDispatchPollResult.init;
    error = null;
    foreach (i; 0 .. pendingComposeJobs.length) {
        if (pendingComposeJobs[i].id != jobId) continue;
        foreach (layerIndex, layerJobId; pendingComposeJobs[i].layerJobIds) {
            if (pendingComposeJobs[i].layerReady[layerIndex]) continue;
            DepthDrawGpuLayerSamplePollResult layerPoll;
            if (!ngPollDepthDrawGpuLayerSample(layerJobId, layerPoll, error)) {
                foreach (remainingLayerIndex, remainingJobId; pendingComposeJobs[i].layerJobIds) {
                    if (!pendingComposeJobs[i].layerReady[remainingLayerIndex]) {
                        cancelDepthDrawGpuLayerSampleJob(remainingJobId);
                    }
                }
                pendingComposeJobs = pendingComposeJobs[0 .. i] ~ pendingComposeJobs[i + 1 .. $];
                return false;
            }
            if (!layerPoll.ready) return true;
            if (layerPoll.readback.layerIndex != layerIndex) {
                error = "DepthDraw GPU compose layer readback index mismatch";
                foreach (remainingLayerIndex, remainingJobId; pendingComposeJobs[i].layerJobIds) {
                    if (!pendingComposeJobs[i].layerReady[remainingLayerIndex]) {
                        cancelDepthDrawGpuLayerSampleJob(remainingJobId);
                    }
                }
                pendingComposeJobs = pendingComposeJobs[0 .. i] ~ pendingComposeJobs[i + 1 .. $];
                return false;
            }
            pendingComposeJobs[i].layerReadbacks[layerIndex] = layerPoll.readback;
            pendingComposeJobs[i].layerReady[layerIndex] = true;
        }
        result.ready = true;
        result.readback = ngBuildDepthDrawGpuComposeReadback(packet, pendingComposeJobs[i].layerReadbacks);
        pendingComposeJobs = pendingComposeJobs[0 .. i] ~ pendingComposeJobs[i + 1 .. $];
        return true;
    }
    error = "DepthDraw GPU compose job was not found";
    return false;
}

private void cancelDepthDrawGpuComposeBackend(uint jobId) {
    foreach (i; 0 .. pendingComposeJobs.length) {
        if (pendingComposeJobs[i].id != jobId) continue;
        foreach (layerIndex, layerJobId; pendingComposeJobs[i].layerJobIds) {
            if (!pendingComposeJobs[i].layerReady[layerIndex])
                cancelDepthDrawGpuLayerSampleJob(layerJobId);
        }
        pendingComposeJobs = pendingComposeJobs[0 .. i] ~ pendingComposeJobs[i + 1 .. $];
        return;
    }
}
} else {
bool ngSubmitDepthDrawGpuLayerSample(
    ref DepthDrawGpuLayerSamplePacket packet,
    out uint jobId,
    out string error
) {
    jobId = 0;
    error = "DepthDraw GPU layer sampling requires the rendering backend";
    return false;
}

bool ngPollDepthDrawGpuLayerSample(
    uint jobId,
    out DepthDrawGpuLayerSamplePollResult result,
    out string error
) {
    result = DepthDrawGpuLayerSamplePollResult.init;
    error = "DepthDraw GPU layer sampling requires the rendering backend";
    return false;
}

size_t ngPendingDepthDrawGpuLayerSampleJobCount() {
    return 0;
}
}

bool ngSubmitDepthDrawGpuCompose(ref DepthDrawGpuComposePacket packet, out uint jobId, out string error) {
    jobId = 0;
    error = null;
    try {
        validateDepthDrawGpuPacket(packet);
        foreach (layer; packet.layers) {
            enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(layer.convolution),
                "DepthDraw GPU composition does not support custom median convolution yet");
        }
    } catch (Exception e) {
        error = e.msg;
        return false;
    }
    if (submitHook !is null) {
        auto submitted = submitHook(packet, jobId, error);
        if (submitted) pendingPackets[jobId] = cloneDepthDrawGpuPacket(packet);
        return submitted;
    }
    version (InDoesRender) {
        auto submitted = submitDepthDrawGpuComposeBackend(packet, jobId, error);
        if (submitted) pendingPackets[jobId] = cloneDepthDrawGpuPacket(packet);
        return submitted;
    }
    error = "DepthDraw GPU composition is unavailable: " ~ ngDepthDrawGpuSupportDiagnostic();
    return false;
}

bool ngPollDepthDrawGpuCompose(uint jobId, out DepthDrawGpuDispatchPollResult result, out string error) {
    result = DepthDrawGpuDispatchPollResult.init;
    error = null;
    if (pollHook !is null) {
        auto polled = pollHook(jobId, result, error);
        if (!polled || !result.ready) return polled;
        auto packet = jobId in pendingPackets;
        if (packet is null) {
            error = "DepthDraw GPU readback has no pending packet";
            return false;
        }
        try {
            validateDepthDrawGpuReadback(*packet, result.readback);
        } catch (Exception e) {
            pendingPackets.remove(jobId);
            error = e.msg;
            return false;
        }
        pendingPackets.remove(jobId);
        return true;
    }
    version (InDoesRender) {
        auto packet = jobId in pendingPackets;
        if (packet is null) {
            error = "DepthDraw GPU readback has no pending packet";
            return false;
        }
        auto polled = pollDepthDrawGpuComposeBackend(jobId, *packet, result, error);
        if (!polled || !result.ready) return polled;
        try {
            validateDepthDrawGpuReadback(*packet, result.readback);
        } catch (Exception e) {
            pendingPackets.remove(jobId);
            error = e.msg;
            return false;
        }
        pendingPackets.remove(jobId);
        return true;
    }
    error = "DepthDraw GPU composition is unavailable: " ~ ngDepthDrawGpuSupportDiagnostic();
    return false;
}

void ngCancelDepthDrawGpuCompose(uint jobId) {
    if (jobId == 0) return;
    version (InDoesRender) {
        if (submitHook is null) cancelDepthDrawGpuComposeBackend(jobId);
    }
    pendingPackets.remove(jobId);
}

size_t ngPendingDepthDrawGpuComposeJobCount() {
    return pendingPackets.length;
}

bool ngSubmitDepthDrawGpuTargetCompose(
    DepthDrawSession session,
    DepthTargetView target,
    int documentWidth,
    int documentHeight,
    out DepthDrawGpuTargetComposeJob job,
    out string error
) {
    job = DepthDrawGpuTargetComposeJob.init;
    error = null;
    auto packet = ngBuildDepthDrawGpuComposePacket(session, target, documentWidth, documentHeight);
    uint jobId;
    if (!ngSubmitDepthDrawGpuCompose(packet, jobId, error)) return false;
    job.jobId = jobId;
    job.packet = cloneDepthDrawGpuPacket(packet);
    return true;
}

bool ngPollDepthDrawGpuTargetCompose(
    ref DepthDrawGpuTargetComposeJob job,
    out DepthDrawGpuTargetComposePollResult result,
    out string error
) {
    result = DepthDrawGpuTargetComposePollResult.init;
    error = null;
    DepthDrawGpuDispatchPollResult dispatchResult;
    if (!ngPollDepthDrawGpuCompose(job.jobId, dispatchResult, error)) return false;
    if (!dispatchResult.ready) return true;
    result.ready = true;
    result.readback = dispatchResult.readback;
    result.result = ngDepthDrawComposeResultFromGpuReadback(job.packet, dispatchResult.readback);
    return true;
}

void ngCancelDepthDrawGpuTargetCompose(ref DepthDrawGpuTargetComposeJob job) {
    ngCancelDepthDrawGpuCompose(job.jobId);
    job = DepthDrawGpuTargetComposeJob.init;
}

float[] ngFlattenDepthDrawGpuLayers(const(DepthDrawGpuLayerPacket)[] layers) {
    float[] values;
    values.length = layers.length * DepthDrawGpuLayerStride;
    foreach (i, layer; layers) {
        auto base = i * DepthDrawGpuLayerStride;
        values[base + DepthDrawGpuLayerField.Width] = cast(float)layer.width;
        values[base + DepthDrawGpuLayerField.Height] = cast(float)layer.height;
        values[base + DepthDrawGpuLayerField.DepthPixelOffset] = cast(float)layer.depthPixelOffset;
        values[base + DepthDrawGpuLayerField.NormalCoverageOffset] = cast(float)layer.normalCoverageOffset;
        values[base + DepthDrawGpuLayerField.Flags] = cast(float)layer.flags;
        values[base + DepthDrawGpuLayerField.BoundsLeft] = layer.boundsLeft;
        values[base + DepthDrawGpuLayerField.BoundsTop] = layer.boundsTop;
        values[base + DepthDrawGpuLayerField.BoundsWidth] = layer.boundsWidth;
        values[base + DepthDrawGpuLayerField.BoundsHeight] = layer.boundsHeight;
        values[base + DepthDrawGpuLayerField.Opacity] = layer.opacity;
        values[base + DepthDrawGpuLayerField.XyOffsetX] = layer.xyOffsetX;
        values[base + DepthDrawGpuLayerField.XyOffsetY] = layer.xyOffsetY;
        values[base + DepthDrawGpuLayerField.XyScaleX] = layer.xyScaleX;
        values[base + DepthDrawGpuLayerField.XyScaleY] = layer.xyScaleY;
        values[base + DepthDrawGpuLayerField.ZOffset] = layer.zOffset;
        values[base + DepthDrawGpuLayerField.ZScale] = layer.zScale;
        values[base + DepthDrawGpuLayerField.BackDepth] = layer.backDepth;
        values[base + DepthDrawGpuLayerField.FrontDepth] = layer.frontDepth;
        values[base + DepthDrawGpuLayerField.AlphaThreshold] = layer.alphaThreshold;
        values[base + DepthDrawGpuLayerField.Channel] = cast(float)layer.channel;
        values[base + DepthDrawGpuLayerField.Convolution] = cast(float)layer.convolution;
        values[base + DepthDrawGpuLayerField.CustomRadius] = cast(float)layer.customRadius;
        values[base + DepthDrawGpuLayerField.SampleDepthScale] = layer.sampleDepthScale;
    }
    return values;
}

float[] ngFlattenDepthDrawGpuBindings(const(DepthDrawGpuBindingPacket)[] bindings) {
    float[] values;
    values.length = bindings.length * DepthDrawGpuBindingStride;
    foreach (i, binding; bindings) {
        auto base = i * DepthDrawGpuBindingStride;
        values[base + DepthDrawGpuBindingField.LayerIndex] = cast(float)binding.layerIndex;
        values[base + DepthDrawGpuBindingField.MergePolicy] = cast(float)binding.mergePolicy;
        values[base + DepthDrawGpuBindingField.Flags] = cast(float)binding.flags;
        values[base + DepthDrawGpuBindingField.CoverageThreshold] = binding.coverageThreshold;
    }
    return values;
}

void ngValidateDepthDrawGpuComposePacket(ref const(DepthDrawGpuComposePacket) packet) {
    validateDepthDrawGpuPacket(packet);
}

void ngValidateDepthDrawGpuComposeReadback(
    ref const(DepthDrawGpuComposePacket) packet,
    ref const(DepthDrawGpuComposeReadback) readback
) {
    validateDepthDrawGpuReadback(packet, readback);
}

DepthDrawGpuComposeReadback ngBuildDepthDrawGpuComposeReadback(
    ref const(DepthDrawGpuComposePacket) packet,
    const(DepthDrawGpuLayerReadback)[] layerReadbacks
) {
    validateDepthDrawGpuPacket(packet);
    enforce(layerReadbacks.length == packet.layers.length,
        "DepthDraw GPU compose readback builder layer count mismatch");

    DepthDrawGpuComposeReadback readback;
    readback.targetGridUuid = packet.targetGridUuid;
    readback.depths = packet.baseDepths.dup;
    readback.winningLayerIndices.length = packet.vertices.length;
    foreach (ref index; readback.winningLayerIndices) index = -1;
    readback.layers.length = layerReadbacks.length;
    bool[] hasComposed;
    hasComposed.length = packet.vertices.length;

    foreach (layerIndex, layerReadback; layerReadbacks) {
        enforce(layerReadback.layerIndex == layerIndex,
            "DepthDraw GPU compose readback builder layer index mismatch");
        enforce(layerReadback.validSamples.length == packet.vertices.length,
            "DepthDraw GPU compose readback builder valid-sample length mismatch");
        enforce(layerReadback.sampleDepths.length == packet.vertices.length,
            "DepthDraw GPU compose readback builder sample-depth length mismatch");
        readback.layers[layerIndex] = DepthDrawGpuLayerReadback(
            layerReadback.layerIndex,
            layerReadback.validSamples.dup,
            layerReadback.sampleDepths.dup);

        auto mergePolicy = cast(DepthMergePolicy)packet.bindings[layerIndex].mergePolicy;
        foreach (vertexIndex, valid; layerReadback.validSamples) {
            enforce(valid == 0 || valid == 1,
                "DepthDraw GPU compose readback builder valid-sample value must be 0 or 1");
            if (valid == 0) continue;
            auto sampleDepth = layerReadback.sampleDepths[vertexIndex];
            enforce(sampleDepth.isFinite,
                "DepthDraw GPU compose readback builder contains non-finite sample depth");
            if (mergeDepthReadback(
                mergePolicy,
                readback.depths[vertexIndex],
                hasComposed[vertexIndex],
                packet.baseDepths[vertexIndex],
                sampleDepth)) {
                readback.winningLayerIndices[vertexIndex] = cast(int)layerIndex;
            }
        }
    }

    validateDepthDrawGpuReadback(packet, readback);
    return readback;
}

private bool mergeDepthReadback(
    DepthMergePolicy policy,
    ref float current,
    ref bool hasComposed,
    float baseDepth,
    float sampledDepth
) {
    final switch (policy) {
        case DepthMergePolicy.Replace:
            current = sampledDepth;
            hasComposed = true;
            return true;
        case DepthMergePolicy.Frontmost:
            if (!hasComposed || sampledDepth > current) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
        case DepthMergePolicy.Backmost:
            if (!hasComposed || sampledDepth < current) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
        case DepthMergePolicy.Add:
            current = (hasComposed ? current : baseDepth) + sampledDepth;
            hasComposed = true;
            return true;
        case DepthMergePolicy.KeepExistingWhereMissing:
            if (!hasComposed) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
    }
}

private void validateDepthDrawGpuPacket(ref const(DepthDrawGpuComposePacket) packet) {
    enforce(packet.vertices.length > 0, "DepthDraw GPU packet has no vertices");
    enforce(packet.vertices.length <= DepthDrawGpuMaxVertices, "DepthDraw GPU packet exceeds maximum vertex count");
    enforce(packet.documentPositions.length == packet.vertices.length,
        "DepthDraw GPU document-position buffer length mismatch");
    enforce(packet.baseDepths.length == packet.vertices.length,
        "DepthDraw GPU base-depth buffer length mismatch");
    enforce(packet.layers.length > 0, "DepthDraw GPU packet has no layers");
    enforce(packet.layers.length <= DepthDrawGpuMaxLayers, "DepthDraw GPU packet exceeds maximum layer count");
    enforce(packet.bindings.length == packet.layers.length,
        "DepthDraw GPU binding/layer count mismatch");

    foreach (i, layer; packet.layers) {
        enforce(layer.width > 0 && layer.height > 0, "DepthDraw GPU layer has invalid dimensions");
        auto pixelCount = cast(size_t)layer.width * cast(size_t)layer.height * 4;
        enforce(cast(size_t)layer.depthPixelOffset + pixelCount <= packet.depthPixels.length,
            "DepthDraw GPU layer depth pixel range is out of bounds");
        if (layer.normalCoverageOffset != uint.max) {
            enforce(cast(size_t)layer.normalCoverageOffset + pixelCount <= packet.normalCoveragePixels.length,
                "DepthDraw GPU layer normal coverage range is out of bounds");
        }
        enforce(layer.xyScaleX.isFinite && layer.xyScaleY.isFinite &&
            layer.zOffset.isFinite && layer.zScale.isFinite &&
            layer.backDepth.isFinite && layer.frontDepth.isFinite &&
            layer.alphaThreshold.isFinite,
            "DepthDraw GPU layer contains non-finite transform or sampling values");
        enforce(isValidDepthDrawGpuChannel(layer.channel), "DepthDraw GPU layer contains invalid channel");
        enforce(isValidDepthDrawGpuConvolution(layer.convolution), "DepthDraw GPU layer contains invalid convolution");
        enforce(layer.customRadius >= 0 && layer.customRadius <= DepthDrawGpuMaxCustomRadius,
            "DepthDraw GPU layer contains invalid custom radius");
        enforce(packet.bindings[i].layerIndex == i, "DepthDraw GPU binding layer index mismatch");
    }
}

private void validateDepthDrawGpuLayerSamplePacket(ref const(DepthDrawGpuLayerSamplePacket) packet) {
    enforce(packet.documentPositions.length > 0, "DepthDraw GPU layer sample packet has no document positions");
    enforce(packet.documentPositions.length <= DepthDrawGpuMaxVertices,
        "DepthDraw GPU layer sample packet exceeds maximum vertex count");
    enforce(packet.layer.width > 0 && packet.layer.height > 0,
        "DepthDraw GPU layer sample packet has invalid layer dimensions");
    enforce(packet.layer.depthPixelOffset == 0,
        "DepthDraw GPU layer sample packet must use zero depth-pixel offset");
    enforce(packet.layer.normalCoverageOffset == 0 || packet.layer.normalCoverageOffset == uint.max,
        "DepthDraw GPU layer sample packet must use zero normal-coverage offset");
    enforce(packet.binding.layerIndex == 0,
        "DepthDraw GPU layer sample binding must reference the zero-offset layer");
    auto pixelCount = cast(size_t)packet.layer.width * cast(size_t)packet.layer.height * DepthDrawGpuRgbaPixelStride;
    enforce(packet.depthPixels.length == pixelCount,
        "DepthDraw GPU layer sample packet has invalid depth pixel length");
    if (packet.layer.normalCoverageOffset != uint.max) {
        enforce(packet.normalCoveragePixels.length == pixelCount,
            "DepthDraw GPU layer sample packet has invalid normal coverage pixel length");
    } else {
        enforce(packet.normalCoveragePixels.length == 0,
            "DepthDraw GPU layer sample packet has unexpected normal coverage pixels");
    }
    enforce(packet.layer.xyScaleX.isFinite && packet.layer.xyScaleY.isFinite &&
        packet.layer.zOffset.isFinite && packet.layer.zScale.isFinite &&
        packet.layer.backDepth.isFinite && packet.layer.frontDepth.isFinite &&
        packet.layer.alphaThreshold.isFinite,
        "DepthDraw GPU layer sample packet contains non-finite transform or sampling values");
    enforce(isValidDepthDrawGpuChannel(packet.layer.channel),
        "DepthDraw GPU layer sample packet contains invalid channel");
    enforce(isValidDepthDrawGpuConvolution(packet.layer.convolution),
        "DepthDraw GPU layer sample packet contains invalid convolution");
    enforce(packet.layer.customRadius >= 0 && packet.layer.customRadius <= DepthDrawGpuMaxCustomRadius,
        "DepthDraw GPU layer sample packet contains invalid custom radius");
}

private bool isValidDepthDrawGpuChannel(int channelValue) {
    if (channelValue < cast(int)DepthImageChannel.AverageRGB ||
        channelValue > cast(int)DepthImageChannel.Luminance) return false;
    auto channel = cast(DepthImageChannel)channelValue;
    switch (channel) {
        case DepthImageChannel.AverageRGB:
        case DepthImageChannel.R:
        case DepthImageChannel.G:
        case DepthImageChannel.B:
        case DepthImageChannel.Luminance:
            return true;
        default:
            return false;
    }
}

private bool isValidDepthDrawGpuConvolution(int convolutionValue) {
    if (convolutionValue < cast(int)DepthImageConvolution.Nearest ||
        convolutionValue > cast(int)DepthImageConvolution.BackmostCustom) return false;
    auto convolution = cast(DepthImageConvolution)convolutionValue;
    switch (convolution) {
        case DepthImageConvolution.Nearest:
        case DepthImageConvolution.Box3x3:
        case DepthImageConvolution.Box5x5:
        case DepthImageConvolution.Gaussian3x3:
        case DepthImageConvolution.Gaussian5x5:
        case DepthImageConvolution.Median3x3:
        case DepthImageConvolution.Frontmost3x3:
        case DepthImageConvolution.Backmost3x3:
        case DepthImageConvolution.BoxCustom:
        case DepthImageConvolution.GaussianCustom:
        case DepthImageConvolution.MedianCustom:
        case DepthImageConvolution.FrontmostCustom:
        case DepthImageConvolution.BackmostCustom:
            return true;
        default:
            return false;
    }
}

private void validateDepthDrawGpuReadback(
    ref const(DepthDrawGpuComposePacket) packet,
    ref const(DepthDrawGpuComposeReadback) readback
) {
    enforce(readback.targetGridUuid == packet.targetGridUuid, "DepthDraw GPU readback target mismatch");
    enforce(readback.depths.length == packet.vertices.length, "DepthDraw GPU readback depth length mismatch");
    enforce(readback.winningLayerIndices.length == packet.vertices.length,
        "DepthDraw GPU readback winner length mismatch");
    enforce(readback.layers.length == packet.layers.length, "DepthDraw GPU readback layer count mismatch");

    foreach (i, depth; readback.depths) {
        enforce(depth.isFinite, "DepthDraw GPU readback contains non-finite final depth");
        auto winner = readback.winningLayerIndices[i];
        enforce(winner >= -1 && winner < cast(int)packet.layers.length,
            "DepthDraw GPU readback winner index is out of bounds");
    }

    foreach (i, layerReadback; readback.layers) {
        enforce(layerReadback.layerIndex == i, "DepthDraw GPU readback layer index mismatch");
        enforce(layerReadback.validSamples.length == packet.vertices.length,
            "DepthDraw GPU readback valid-sample length mismatch");
        enforce(layerReadback.sampleDepths.length == packet.vertices.length,
            "DepthDraw GPU readback sample-depth length mismatch");
        foreach (j, valid; layerReadback.validSamples) {
            enforce(valid == 0 || valid == 1, "DepthDraw GPU readback valid-sample value must be 0 or 1");
            if (valid != 0) {
                enforce(layerReadback.sampleDepths[j].isFinite,
                    "DepthDraw GPU readback contains non-finite sample depth");
            }
        }
    }
}

private DepthDrawGpuComposePacket cloneDepthDrawGpuPacket(ref const(DepthDrawGpuComposePacket) packet) {
    DepthDrawGpuComposePacket clone;
    clone.targetGridUuid = packet.targetGridUuid;
    clone.documentWidth = packet.documentWidth;
    clone.documentHeight = packet.documentHeight;
    clone.vertices = packet.vertices.dup;
    clone.documentPositions = packet.documentPositions.dup;
    clone.baseDepths = packet.baseDepths.dup;
    clone.layers = packet.layers.dup;
    clone.bindings = packet.bindings.dup;
    clone.depthPixels = packet.depthPixels.dup;
    clone.normalCoveragePixels = packet.normalCoveragePixels.dup;
    return clone;
}

vec2 ngDepthDrawGpuLayerPixelFromDocument(DepthDrawGpuLayerPacket layer, vec2 documentPoint) {
    auto scaleX = layer.xyScaleX == 0.0f ? 1.0f : layer.xyScaleX;
    auto scaleY = layer.xyScaleY == 0.0f ? 1.0f : layer.xyScaleY;
    return vec2(
        (documentPoint.x - layer.boundsLeft - layer.xyOffsetX) / scaleX,
        (documentPoint.y - layer.boundsTop - layer.xyOffsetY) / scaleY
    );
}

DepthDrawComposeResult ngDepthDrawComposeResultFromGpuReadback(
    const(DepthDrawGpuComposePacket) packet,
    const(DepthDrawGpuComposeReadback) readback
) {
    DepthDrawComposeResult result;
    result.targetGridUuid = readback.targetGridUuid;
    result.depths = readback.depths.dup;
    result.winningLayerIds.length = readback.winningLayerIndices.length;

    foreach (i, index; readback.winningLayerIndices) {
        if (index >= 0 && cast(size_t)index < packet.layers.length) {
            result.winningLayerIds[i] = packet.layers[cast(size_t)index].layerId;
        }
    }

    foreach (layerReadback; readback.layers) {
        DepthDrawLayerComposeStats stats;
        if (layerReadback.layerIndex < packet.layers.length) {
            auto layer = packet.layers[layerReadback.layerIndex];
            stats.layerId = layer.layerId;
            if (layerReadback.layerIndex < packet.bindings.length) {
                stats.additiveMerge = packet.bindings[layerReadback.layerIndex].mergePolicy == cast(uint)DepthMergePolicy.Add;
            }
        }

        auto count = min(layerReadback.validSamples.length, layerReadback.sampleDepths.length);
        foreach (i; 0 .. count) {
            if (layerReadback.validSamples[i] == 0) {
                stats.missingVertices++;
                result.missingVertices++;
                continue;
            }
            auto depth = layerReadback.sampleDepths[i];
            stats.sampledVertices++;
            result.sampledVertices++;
            includeDepthRange(stats, depth);
            if (stats.additiveMerge) stats.contributedVertices++;
        }
        if (layerReadback.validSamples.length > count) {
            stats.missingVertices += layerReadback.validSamples.length - count;
            result.missingVertices += layerReadback.validSamples.length - count;
        }

        result.layerStats ~= stats;
    }

    foreach (winnerId; result.winningLayerIds) {
        if (winnerId.length == 0) continue;
        foreach (ref stats; result.layerStats) {
            if (stats.layerId == winnerId) {
                stats.winningVertices++;
                if (!stats.additiveMerge) stats.contributedVertices++;
                break;
            }
        }
    }

    foreach (depth; result.depths) includeDepthRange(result, depth);
    return result;
}

private void includeDepthRange(ref DepthDrawLayerComposeStats stats, float depth) {
    if (!depth.isFinite) return;
    if (!stats.hasDepthRange) {
        stats.hasDepthRange = true;
        stats.minDepth = depth;
        stats.maxDepth = depth;
        return;
    }
    stats.minDepth = min(stats.minDepth, depth);
    stats.maxDepth = max(stats.maxDepth, depth);
}

private void includeDepthRange(ref DepthDrawComposeResult result, float depth) {
    if (!depth.isFinite) return;
    if (!result.hasDepthRange) {
        result.hasDepthRange = true;
        result.minDepth = depth;
        result.maxDepth = depth;
        return;
    }
    result.minDepth = min(result.minDepth, depth);
    result.maxDepth = max(result.maxDepth, depth);
}

private struct ComposeLayer {
    DepthDrawLayer layer;
    DepthDrawBinding binding;
    size_t bindingIndex;
}

DepthDrawGpuComposePacket ngBuildDepthDrawGpuComposePacket(
    DepthDrawSession session,
    DepthTargetView target,
    int documentWidth,
    int documentHeight
) {
    DepthDrawGpuComposePacket packet;
    if (session is null || target is null) return packet;

    auto grid = target.getTarget();
    packet.targetGridUuid = grid.uuid;
    packet.documentWidth = documentWidth;
    packet.documentHeight = documentHeight;
    packet.vertices = target.getVertices();
    packet.documentPositions.length = packet.vertices.length;
    foreach (i, vertex; packet.vertices) {
        packet.documentPositions[i] = ngDepthDrawVertexDocumentPosition(target, vertex, documentWidth, documentHeight);
    }

    packet.baseDepths = target.baseDepths.dup;
    if (packet.baseDepths.length != packet.vertices.length) {
        auto oldLength = packet.baseDepths.length;
        packet.baseDepths.length = packet.vertices.length;
        foreach (i; oldLength .. packet.baseDepths.length) packet.baseDepths[i] = 0.0f;
    }

    ComposeLayer[] composeLayers;
    foreach (bindingIndex, binding; session.bindingsForGrid(grid.uuid)) {
        auto layerPtr = session.layerById(binding.layerId);
        if (layerPtr is null || !layerPtr.enabled || !layerPtr.visible || !layerPtr.hasDepthPixels()) continue;
        ComposeLayer entry;
        entry.layer = *layerPtr;
        entry.binding = binding;
        entry.bindingIndex = bindingIndex;
        composeLayers ~= entry;
    }
    // Keep the GPU upload order identical to ngComposeDepthDrawTarget's CPU layer order.
    sort!((a, b) => a.binding.order == b.binding.order
        ? a.bindingIndex < b.bindingIndex
        : a.binding.order < b.binding.order)(composeLayers);

    foreach (composeLayer; composeLayers) {
        auto layerIndex = cast(uint)packet.layers.length;
        auto layer = composeLayer.layer;
        auto binding = composeLayer.binding;
        auto settings = layer.sampleSettings();

        DepthDrawGpuLayerPacket layerPacket;
        layerPacket.layerId = layer.id;
        layerPacket.width = cast(uint)layer.width;
        layerPacket.height = cast(uint)layer.height;
        layerPacket.depthPixelOffset = cast(uint)packet.depthPixels.length;
        layerPacket.normalCoverageOffset = layer.hasNormalCoverage()
            ? cast(uint)packet.normalCoveragePixels.length
            : uint.max;
        layerPacket.flags = (layer.invert ? 1u : 0u) | (layer.hasNormalCoverage() ? 2u : 0u);
        layerPacket.boundsLeft = cast(float)layer.bounds.left;
        layerPacket.boundsTop = cast(float)layer.bounds.top;
        layerPacket.boundsWidth = cast(float)layer.bounds.width;
        layerPacket.boundsHeight = cast(float)layer.bounds.height;
        layerPacket.opacity = layer.opacity;
        layerPacket.xyOffsetX = layer.xyOffset.x;
        layerPacket.xyOffsetY = layer.xyOffset.y;
        layerPacket.xyScaleX = layer.xyScale.x;
        layerPacket.xyScaleY = layer.xyScale.y;
        layerPacket.zOffset = layer.zOffset;
        layerPacket.zScale = layer.zScale;
        layerPacket.backDepth = settings.backDepth;
        layerPacket.frontDepth = settings.frontDepth;
        layerPacket.sampleDepthScale = layer.sampleDepthScale;
        layerPacket.alphaThreshold = settings.alphaThreshold;
        layerPacket.channel = cast(int)settings.channel;
        layerPacket.convolution = cast(int)settings.convolution;
        layerPacket.customRadius = settings.customRadius;
        packet.layers ~= layerPacket;
        packet.depthPixels ~= layer.depthPixels;
        if (layer.hasNormalCoverage()) packet.normalCoveragePixels ~= layer.normalCoverage;

        DepthDrawGpuBindingPacket bindingPacket;
        bindingPacket.layerId = layer.id;
        bindingPacket.layerIndex = layerIndex;
        bindingPacket.mergePolicy = cast(uint)binding.mergePolicy;
        bindingPacket.flags = binding.useNormalLayerAlpha ? 1u : 0u;
        bindingPacket.coverageThreshold = binding.coverageThreshold;
        packet.bindings ~= bindingPacket;
    }

    return packet;
}
