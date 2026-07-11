module nijigenerate.commands.depth.bone_gpu_async;

import nijilive.math : Vec2Array, mat4;

version (InDoesRender) {
import bindbc.opengl;
import std.algorithm.comparison : min;
import std.exception : enforce;
import std.string : toStringz;
}

enum NgDepthBoneGpuAsyncMaxBones = 64u;
enum NgDepthBoneGpuAsyncMaxSources = 128u;
enum NgDepthBoneGpuAsyncMaxInfluences = 8u;
enum NgDepthBoneGpuAsyncMaxVertices = 1_000_000u;
enum NgDepthBoneGpuAsyncBoneStride = 24u;
enum NgDepthBoneGpuAsyncSourceStride = 8u;

struct DepthBoneGpuDispatchPacket {
    Vec2Array vertices;
    float[] depths;
    float[] bones;
    float[] sources;
    mat4 targetToRoot;
    mat4 rootToTarget;
    float influenceRadiusFloor;
    float radiusScale;
    uint boneCount;
    uint sourceCount;
    uint maxInfluences;
}

struct NgDepthBoneGpuAsyncResult {
    bool ready;
    float[] xs;
    float[] ys;
}

alias NgDepthBoneGpuAsyncSupportHook = bool function();
alias NgDepthBoneGpuAsyncSubmitHook = bool function(ref DepthBoneGpuDispatchPacket packet, out uint jobId, out string error);
alias NgDepthBoneGpuAsyncPollHook = bool function(uint jobId, out NgDepthBoneGpuAsyncResult result, out string error);

private NgDepthBoneGpuAsyncSupportHook supportHook;
private NgDepthBoneGpuAsyncSubmitHook submitHook;
private NgDepthBoneGpuAsyncPollHook pollHook;

void ngSetDepthBoneGpuAsyncTestHooks(
    NgDepthBoneGpuAsyncSupportHook support,
    NgDepthBoneGpuAsyncSubmitHook submit,
    NgDepthBoneGpuAsyncPollHook poll
) {
    supportHook = support;
    submitHook = submit;
    pollHook = poll;
}

void ngClearDepthBoneGpuAsyncTestHooks() {
    supportHook = null;
    submitHook = null;
    pollHook = null;
}

version (InDoesRender) {

private __gshared GLuint depthBoneProgram;
private __gshared GLint uVertexCount = -1;
private __gshared GLint uBoneCount = -1;
private __gshared GLint uSourceCount = -1;
private __gshared GLint uMaxInfluences = -1;
private __gshared GLint uInfluenceRadiusFloor = -1;
private __gshared GLint uRadiusScale = -1;
private __gshared GLint uTargetToRoot = -1;
private __gshared GLint uRootToTarget = -1;
private __gshared GLint uBones = -1;
private __gshared GLint uSources = -1;
private __gshared uint nextJobId = 1;

private struct PendingDepthBoneGpuJob {
    uint id;
    size_t count;
    GLuint verticesBuffer;
    GLuint depthsBuffer;
    GLuint bonesBuffer;
    GLuint sourcesBuffer;
    GLuint outputBuffer;
    GLuint bonesTexture;
    GLuint sourcesTexture;
    GLuint vertexArray;
    GLsync fence;
}

private __gshared PendingDepthBoneGpuJob[] pendingJobs;

private enum string VertexSource = q"GLSL
#version 330

#define MAX_INFLUENCES 8u
#define BONE_STRIDE 24u
#define SOURCE_STRIDE 8u

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
    inout float scores[8],
    inout float distances[8],
    inout uint boneIndices[8],
    inout vec3 rests[8],
    inout uint count,
    float score,
    float distanceSq,
    uint boneIndex,
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
        rests[pos] = rests[prev];
        pos = prev;
    }
    scores[pos] = score;
    distances[pos] = distanceSq;
    boneIndices[pos] = boneIndex;
    rests[pos] = rest;
}

void main() {
    float x = inX;
    float y = inY;
    float z = inDepth;

    float scores[8];
    float distances[8];
    uint boneIndices[8];
    vec3 rests[8];
    uint influenceCount = 0u;

    int lockedTerminal = -1;
    float lockedScore = 0.0;
    float lockedDistance = 0.0;
    uint lockedBoneIndex = 0u;
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

        vec3 sourceRestLocal = vec3(x, y, z * depthScale + depthOffset);
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
                lockedRest = sourceRest;
            }
            continue;
        }

        insertInfluence(scores, distances, boneIndices, rests, influenceCount, score, distanceSq, boneIndex, sourceRest);
    }

    if (lockedTerminal >= 0) {
        influenceCount = 1u;
        scores[0] = 1.0;
        boneIndices[0] = lockedBoneIndex;
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

    vec3 deformed = vec3(0.0);
    for (uint j = 0u; j < influenceCount; ++j) {
        uint boneIndex = boneIndices[j];
        float w = scores[j] / total;
        deformed += (boneSkinMatrix(boneIndex) * vec4(rests[j], 1.0)).xyz * w;
    }

    vec3 local = (rootToTarget * vec4(deformed, 1.0)).xyz;
    outDeformX = local.x - x;
    outDeformY = local.y - y;
}
GLSL";

bool ngDepthBoneGpuAsyncSupported() {
    if (supportHook !is null) return supportHook();
    return ngDepthBoneGpuAsyncMissingRequirements().length == 0;
}

string[] ngDepthBoneGpuAsyncMissingRequirements() {
    string[] missing;
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
    return missing;
}

private void checkShader(GLuint shader, string label) {
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

private void checkProgram(GLuint program) {
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
    enforce(false, "DepthBone async transform feedback shader link failed: " ~ log);
}

private void ensureProgram() {
    if (depthBoneProgram != 0) return;
    auto shader = glCreateShader(GL_VERTEX_SHADER);
    auto src = VertexSource.toStringz;
    glShaderSource(shader, 1, &src, null);
    glCompileShader(shader);
    checkShader(shader, "DepthBone async transform feedback shader");

    depthBoneProgram = glCreateProgram();
    glAttachShader(depthBoneProgram, shader);
    const(char)*[2] varyings = ["outDeformX".toStringz, "outDeformY".toStringz];
    glTransformFeedbackVaryings(depthBoneProgram, cast(GLsizei)varyings.length, varyings.ptr, GL_SEPARATE_ATTRIBS);
    glLinkProgram(depthBoneProgram);
    glDetachShader(depthBoneProgram, shader);
    glDeleteShader(shader);
    checkProgram(depthBoneProgram);

    uVertexCount = glGetUniformLocation(depthBoneProgram, "vertexCount");
    uBoneCount = glGetUniformLocation(depthBoneProgram, "boneCount");
    uSourceCount = glGetUniformLocation(depthBoneProgram, "sourceCount");
    uMaxInfluences = glGetUniformLocation(depthBoneProgram, "maxInfluences");
    uInfluenceRadiusFloor = glGetUniformLocation(depthBoneProgram, "influenceRadiusFloor");
    uRadiusScale = glGetUniformLocation(depthBoneProgram, "radiusScale");
    uTargetToRoot = glGetUniformLocation(depthBoneProgram, "targetToRoot");
    uRootToTarget = glGetUniformLocation(depthBoneProgram, "rootToTarget");
    uBones = glGetUniformLocation(depthBoneProgram, "bones");
    uSources = glGetUniformLocation(depthBoneProgram, "sources");
}

private GLuint createBuffer(GLenum target, const(void)[] bytes, GLenum usage = GL_DYNAMIC_DRAW) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, cast(GLsizeiptr)bytes.length, bytes.ptr, usage);
    return buffer;
}

private void createTextureBuffer(ref GLuint buffer, ref GLuint texture, const(float)[] data) {
    buffer = createBuffer(GL_TEXTURE_BUFFER, cast(const(void)[])data);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_BUFFER, texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

private float[] packVertexLanes(ref DepthBoneGpuDispatchPacket packet) {
    auto count = packet.vertices.length;
    float[] lanes;
    lanes.length = count * 2;
    foreach (i; 0 .. count) {
        auto vertex = packet.vertices[i];
        lanes[i] = vertex.x;
        lanes[i + count] = vertex.y;
    }
    return lanes;
}

private void deleteJobResources(ref PendingDepthBoneGpuJob job) {
    if (job.fence !is null && glDeleteSync !is null) {
        glDeleteSync(job.fence);
        job.fence = null;
    }
    GLuint[5] buffers = [
        job.verticesBuffer,
        job.depthsBuffer,
        job.bonesBuffer,
        job.sourcesBuffer,
        job.outputBuffer,
    ];
    bool hasBuffer;
    foreach (buffer; buffers) hasBuffer = hasBuffer || buffer != 0;
    if (hasBuffer && glDeleteBuffers !is null) glDeleteBuffers(cast(GLsizei)buffers.length, buffers.ptr);
    GLuint[2] textures = [job.bonesTexture, job.sourcesTexture];
    bool hasTexture;
    foreach (texture; textures) hasTexture = hasTexture || texture != 0;
    if (hasTexture && glDeleteTextures !is null) glDeleteTextures(cast(GLsizei)textures.length, textures.ptr);
    if (job.vertexArray != 0 && glDeleteVertexArrays !is null) glDeleteVertexArrays(1, &job.vertexArray);
    job = PendingDepthBoneGpuJob.init;
}

private void validatePacket(ref DepthBoneGpuDispatchPacket packet) {
    enforce(packet.vertices.length > 0, "DepthBone GPU packet has no vertices");
    enforce(packet.vertices.length <= NgDepthBoneGpuAsyncMaxVertices, "DepthBone GPU packet exceeds maximum vertex count");
    enforce(packet.depths.length >= packet.vertices.length, "DepthBone GPU depth buffer is too short");
    enforce(packet.boneCount > 0, "DepthBone GPU packet has no bones");
    enforce(packet.sourceCount > 0, "DepthBone GPU packet has no sources");
    enforce(packet.boneCount <= NgDepthBoneGpuAsyncMaxBones, "DepthBone GPU packet exceeds maximum bone count");
    enforce(packet.sourceCount <= NgDepthBoneGpuAsyncMaxSources, "DepthBone GPU packet exceeds maximum source count");
    enforce(packet.maxInfluences <= NgDepthBoneGpuAsyncMaxInfluences, "DepthBone GPU packet exceeds maximum influence count");
    enforce(packet.bones.length >= packet.boneCount * NgDepthBoneGpuAsyncBoneStride, "DepthBone GPU bone buffer is too short");
    enforce(packet.sources.length >= packet.sourceCount * NgDepthBoneGpuAsyncSourceStride, "DepthBone GPU source buffer is too short");
}

bool ngSubmitDepthBoneGpuAsync(ref DepthBoneGpuDispatchPacket packet, out uint jobId, out string error) {
    jobId = 0;
    error = null;
    if (submitHook !is null) return submitHook(packet, jobId, error);
    PendingDepthBoneGpuJob job;
    bool queued;
    try {
        enforce(ngDepthBoneGpuAsyncSupported(), "DepthBone GPU async deformation requires transform feedback and sync support");
        validatePacket(packet);
        ensureProgram();

        GLint previousProgram;
        GLint previousVertexArray;
        GLint previousArrayBuffer;
        GLint previousTexture0Buffer;
        GLint previousTexture1Buffer;
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
        glActiveTexture(cast(GLenum)previousActiveTexture);
        scope(exit) {
            glBindBuffer(GL_ARRAY_BUFFER, cast(GLuint)previousArrayBuffer);
            glBindVertexArray(cast(GLuint)previousVertexArray);
            glUseProgram(cast(GLuint)previousProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture0Buffer);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, cast(GLuint)previousTexture1Buffer);
            glActiveTexture(cast(GLenum)previousActiveTexture);
            if (rasterizerDiscardWasEnabled == GL_TRUE) glEnable(GL_RASTERIZER_DISCARD);
            else glDisable(GL_RASTERIZER_DISCARD);
        }

        job.id = nextJobId++;
        job.count = packet.vertices.length;
        glGenVertexArrays(1, &job.vertexArray);
        glBindVertexArray(job.vertexArray);
        auto vertexLanes = packVertexLanes(packet);
        job.verticesBuffer = createBuffer(GL_ARRAY_BUFFER, cast(const(void)[])vertexLanes);
        job.depthsBuffer = createBuffer(GL_ARRAY_BUFFER, cast(const(void)[])packet.depths);
        createTextureBuffer(job.bonesBuffer, job.bonesTexture, packet.bones);
        createTextureBuffer(job.sourcesBuffer, job.sourcesTexture, packet.sources);

        auto count = packet.vertices.length;
        auto laneBytes = cast(GLsizeiptr)(count * float.sizeof);
        glGenBuffers(1, &job.outputBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, job.outputBuffer);
        glBufferData(GL_ARRAY_BUFFER, laneBytes * 2, null, GL_STREAM_READ);

        glUseProgram(depthBoneProgram);
        glUniform1ui(uVertexCount, cast(GLuint)packet.vertices.length);
        glUniform1ui(uBoneCount, packet.boneCount);
        glUniform1ui(uSourceCount, packet.sourceCount);
        glUniform1ui(uMaxInfluences, min(packet.maxInfluences == 0 ? 1u : packet.maxInfluences, NgDepthBoneGpuAsyncMaxInfluences));
        glUniform1f(uInfluenceRadiusFloor, packet.influenceRadiusFloor);
        glUniform1f(uRadiusScale, packet.radiusScale);
        glUniformMatrix4fv(uTargetToRoot, 1, GL_TRUE, packet.targetToRoot.ptr);
        glUniformMatrix4fv(uRootToTarget, 1, GL_TRUE, packet.rootToTarget.ptr);
        glUniform1i(uBones, 0);
        glUniform1i(uSources, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, job.bonesTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, job.sourcesTexture);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, job.verticesBuffer);
        glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, cast(void*)0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, job.verticesBuffer);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, cast(void*)(cast(GLintptr)(packet.vertices.length * float.sizeof)));

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, job.depthsBuffer);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, cast(void*)0);

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
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);

        pendingJobs ~= job;
        queued = true;
        jobId = job.id;
        return true;
    } catch (Exception e) {
        if (!queued) deleteJobResources(job);
        error = e.msg;
        return false;
    }
}

private bool pollDepthBoneGpuAsync(uint jobId, ulong timeoutNanoseconds, out NgDepthBoneGpuAsyncResult result, out string error) {
    result = NgDepthBoneGpuAsyncResult.init;
    error = null;
    if (pollHook !is null) return pollHook(jobId, result, error);
    foreach (i; 0 .. pendingJobs.length) {
        if (pendingJobs[i].id != jobId) continue;
        auto flags = cast(GLbitfield)(timeoutNanoseconds > 0 ? GL_SYNC_FLUSH_COMMANDS_BIT : 0);
        auto status = glClientWaitSync(pendingJobs[i].fence, flags, timeoutNanoseconds);
        if (status == GL_TIMEOUT_EXPIRED) return true;
        if (status == GL_WAIT_FAILED) {
            error = "DepthBone GPU fence wait failed";
            deleteJobResources(pendingJobs[i]);
            pendingJobs = pendingJobs[0 .. i] ~ pendingJobs[i + 1 .. $];
            return false;
        }

        result.ready = true;
        result.xs.length = pendingJobs[i].count;
        result.ys.length = pendingJobs[i].count;
        auto byteCount = cast(GLsizeiptr)(pendingJobs[i].count * float.sizeof);
        auto yOffset = cast(GLintptr)(pendingJobs[i].count * float.sizeof);
        GLint previousArrayBuffer;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, pendingJobs[i].outputBuffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, byteCount, result.xs.ptr);
        glGetBufferSubData(GL_ARRAY_BUFFER, yOffset, byteCount, result.ys.ptr);
        glBindBuffer(GL_ARRAY_BUFFER, cast(GLuint)previousArrayBuffer);
        deleteJobResources(pendingJobs[i]);
        pendingJobs = pendingJobs[0 .. i] ~ pendingJobs[i + 1 .. $];
        return true;
    }
    error = "DepthBone GPU async job was not found";
    return false;
}

bool ngPollDepthBoneGpuAsync(uint jobId, out NgDepthBoneGpuAsyncResult result, out string error) {
    return pollDepthBoneGpuAsync(jobId, 0, result, error);
}

size_t ngPendingDepthBoneGpuAsyncJobCount() {
    return pendingJobs.length;
}

} else {

bool ngDepthBoneGpuAsyncSupported() {
    if (supportHook !is null) return supportHook();
    return false;
}

string[] ngDepthBoneGpuAsyncMissingRequirements() {
    return ["rendering backend"];
}

bool ngSubmitDepthBoneGpuAsync(ref DepthBoneGpuDispatchPacket packet, out uint jobId, out string error) {
    jobId = 0;
    error = "DepthBone GPU async deformation requires the rendering backend";
    if (submitHook !is null) return submitHook(packet, jobId, error);
    return false;
}

bool ngPollDepthBoneGpuAsync(uint jobId, out NgDepthBoneGpuAsyncResult result, out string error) {
    result = NgDepthBoneGpuAsyncResult.init;
    error = "DepthBone GPU async deformation requires the rendering backend";
    if (pollHook !is null) return pollHook(jobId, result, error);
    return false;
}

size_t ngPendingDepthBoneGpuAsyncJobCount() {
    return 0;
}

}
