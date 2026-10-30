#include "gpu_internal.hpp"
#include "gpu_shader.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <span>
#include <common/shared_array.hpp>
#define glActiveTexture ::nijigenerate::platform::ngGLBackendProcedure<PFNGLACTIVETEXTUREPROC>("glActiveTexture")
#define glAttachShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLATTACHSHADERPROC>("glAttachShader")
#define glBeginTransformFeedback ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBEGINTRANSFORMFEEDBACKPROC>("glBeginTransformFeedback")
#define glBindBuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")
#define glBindBufferBase ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDBUFFERBASEPROC>("glBindBufferBase")
#define glBindBufferRange ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDBUFFERRANGEPROC>("glBindBufferRange")
#define glBindTexture ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glBindTexture)>("glBindTexture")
#define glBindVertexArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray")
#define glBufferData ::nijigenerate::platform::ngGLBackendProcedure<PFNGLBUFFERDATAPROC>("glBufferData")
#define glClientWaitSync ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCLIENTWAITSYNCPROC>("glClientWaitSync")
#define glCompileShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCOMPILESHADERPROC>("glCompileShader")
#define glCreateProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCREATEPROGRAMPROC>("glCreateProgram")
#define glCreateShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLCREATESHADERPROC>("glCreateShader")
#define glDeleteBuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers")
#define glDeleteShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETESHADERPROC>("glDeleteShader")
#define glDeleteSync ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETESYNCPROC>("glDeleteSync")
#define glDeleteTextures ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDeleteTextures)>("glDeleteTextures")
#define glDeleteVertexArrays ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays")
#define glDetachShader ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDETACHSHADERPROC>("glDetachShader")
#define glDisable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDisable)>("glDisable")
#define glDisableVertexAttribArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray")
#define glDrawArrays ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glDrawArrays)>("glDrawArrays")
#define glEnable ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glEnable)>("glEnable")
#define glEnableVertexAttribArray ::nijigenerate::platform::ngGLBackendProcedure<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray")
#define glEndTransformFeedback ::nijigenerate::platform::ngGLBackendProcedure<PFNGLENDTRANSFORMFEEDBACKPROC>("glEndTransformFeedback")
#define glFenceSync ::nijigenerate::platform::ngGLBackendProcedure<PFNGLFENCESYNCPROC>("glFenceSync")
#define glFlush ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glFlush)>("glFlush")
#define glGenBuffers ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENBUFFERSPROC>("glGenBuffers")
#define glGenTextures ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGenTextures)>("glGenTextures")
#define glGenVertexArrays ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays")
#define glGetBufferSubData ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETBUFFERSUBDATAPROC>("glGetBufferSubData")
#define glGetInteger64i_v ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETINTEGER64I_VPROC>("glGetInteger64i_v")
#define glGetIntegeri_v ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETINTEGERI_VPROC>("glGetIntegeri_v")
#define glGetIntegerv ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glGetIntegerv)>("glGetIntegerv")
#define glGetProgramInfoLog ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog")
#define glGetProgramiv ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETPROGRAMIVPROC>("glGetProgramiv")
#define glGetShaderInfoLog ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog")
#define glGetShaderiv ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETSHADERIVPROC>("glGetShaderiv")
#define glGetUniformLocation ::nijigenerate::platform::ngGLBackendProcedure<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation")
#define glIsEnabled ::nijigenerate::platform::ngGLBackendProcedure<decltype(&::glIsEnabled)>("glIsEnabled")
#define glLinkProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLLINKPROGRAMPROC>("glLinkProgram")
#define glShaderSource ::nijigenerate::platform::ngGLBackendProcedure<PFNGLSHADERSOURCEPROC>("glShaderSource")
#define glTexBuffer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLTEXBUFFERPROC>("glTexBuffer")
#define glTransformFeedbackVaryings ::nijigenerate::platform::ngGLBackendProcedure<PFNGLTRANSFORMFEEDBACKVARYINGSPROC>("glTransformFeedbackVaryings")
#define glUniform1i ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUNIFORM1IPROC>("glUniform1i")
#define glUseProgram ::nijigenerate::platform::ngGLBackendProcedure<PFNGLUSEPROGRAMPROC>("glUseProgram")
#define glVertexAttribPointer ::nijigenerate::platform::ngGLBackendProcedure<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer")
namespace nijigenerate::viewport::depth::draw {
using namespace gpu_detail;
namespace gpu_detail {
namespace {
// These variables are shared, matching the original __gshared GL state.
GLuint program = 0;
GLint layerUniform = -1, bindingUniform = -1, depthUniform = -1, coverageUniform = -1;
std::uint32_t nextSampleId = 1, nextComposeId = 1;
constexpr GLenum textureBufferBinding = 0x8C2A;
struct SampleJob {
    std::uint32_t id = 0, layerIndex = 0;
    std::size_t count = 0;
    GLuint positions = 0, layer = 0, binding = 0, depth = 0, coverage = 0, output = 0;
    GLuint layerTexture = 0, bindingTexture = 0, depthTexture = 0, coverageTexture = 0, vao = 0;
    GLsync fence = nullptr;
};
struct ComposeJob {
    std::uint32_t id = 0;
    compat::Slice<std::uint32_t> ids;
    compat::Slice<DepthDrawGpuLayerReadback> readbacks;
    compat::Slice<bool> ready;
};
compat::Slice<SampleJob> sampleJobs;
compat::Slice<ComposeJob> composeJobs;
template<class T> void removeJob(compat::Slice<T>& jobs, std::size_t index, std::size_t sourceLine = 0) {
    if (sourceLine && index > jobs.size())
        throw compat::ngStaticError<compat::ArraySliceError>(0, index, jobs.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), sourceLine);
    const auto prefix = jobs.slice(0, index);
    if (sourceLine && index + 1 > jobs.size())
        throw compat::ngStaticError<compat::ArraySliceError>(index + 1, jobs.size(), jobs.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), sourceLine);
    const auto suffix = jobs.slice(index + 1, jobs.size());
    compat::Slice<T> remaining;
    const auto count = prefix.size() + suffix.size();
    if (count) remaining.resize(count);
    std::size_t next = 0;
    for (const auto& job : prefix) remaining[next++] = job;
    for (const auto& job : suffix) remaining[next++] = job;
    jobs = remaining;
}
struct IndexedBinding { GLint buffer = 0; GLint64 start = 0, size = 0; };
IndexedBinding captureBinding(GLuint index) {
    IndexedBinding s;
    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, index, &s.buffer);
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_START, index, &s.start);
    glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, index, &s.size);
    return s;
}
void restoreBinding(GLuint index, IndexedBinding s) {
    if (!s.buffer) glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, 0);
    else if (s.size > 0) glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index,
        static_cast<GLuint>(s.buffer), static_cast<GLintptr>(s.start), static_cast<GLsizeiptr>(s.size));
    else glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, static_cast<GLuint>(s.buffer));
}
void checkShader(GLuint shader, compat::RetainedStringSlice label) {
    GLint status = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &status); if (status == GL_TRUE) return;
    GLint length = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log;
    if (length > 1) {
        allocateGpuMemory([&] { log.resize(static_cast<std::size_t>(length), static_cast<char>(0xff)); });
        glGetShaderInfoLog(shader, length, nullptr, log.data());
    }
    auto message = allocateGpuMemory([&] { return std::string(label.view()) + " compile failed: " + log; });
    enforce(false, std::move(message), 663);
}
void checkProgram(GLuint value) {
    GLint status = 0; glGetProgramiv(value, GL_LINK_STATUS, &status); if (status == GL_TRUE) return;
    GLint length = 0; glGetProgramiv(value, GL_INFO_LOG_LENGTH, &length);
    std::string log;
    if (length > 1) {
        allocateGpuMemory([&] { log.resize(static_cast<std::size_t>(length), static_cast<char>(0xff)); });
        glGetProgramInfoLog(value, length, nullptr, log.data());
    }
    auto message = allocateGpuMemory([&] {
        return "DepthDraw layer sample transform feedback shader link failed: " + log;
    });
    enforce(false, std::move(message), 677);
}
void ensureProgram() {
    if (program) return;
    auto shader = glCreateShader(GL_VERTEX_SHADER);
    const compat::SourceCString sourceText(compat::RetainedString::fromStaticLiteral(layerSampleVertexSource));
    compat::detail::ngSourceCStringPostconditions(layerSampleVertexSource, sourceText.data());
    const char* source = sourceText.data();
    glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
    checkShader(shader, compat::RetainedString::fromStaticLiteral("DepthDraw layer sample transform feedback shader"));
    program = glCreateProgram(); glAttachShader(program, shader);
    const compat::SourceCString validName(compat::RetainedString::fromStaticLiteral("outValid"));
    compat::detail::ngSourceCStringPostconditions("outValid", validName.data());
    const compat::SourceCString depthName(compat::RetainedString::fromStaticLiteral("outDepth"));
    compat::detail::ngSourceCStringPostconditions("outDepth", depthName.data());
    const char* varyings[]{validName.data(), depthName.data()};
    glTransformFeedbackVaryings(program, 2, varyings, GL_SEPARATE_ATTRIBS);
    glLinkProgram(program); glDetachShader(program, shader); glDeleteShader(shader); checkProgram(program);
    layerUniform = glGetUniformLocation(program, "layerData");
    bindingUniform = glGetUniformLocation(program, "bindingData");
    depthUniform = glGetUniformLocation(program, "depthPixels");
    coverageUniform = glGetUniformLocation(program, "normalCoveragePixels");
}
template<class Range> GLuint buffer(GLenum target, Range data, GLenum usage = GL_DYNAMIC_DRAW) {
    GLuint value = 0; glGenBuffers(1, &value); glBindBuffer(target, value);
    glBufferData(target, static_cast<GLsizeiptr>(data.size() * sizeof(*data.data())), data.data(), usage); return value;
}
template<class Range> void texture(GLuint& b, GLuint& t, Range data, GLenum format) {
    b = buffer(GL_TEXTURE_BUFFER, data); glGenTextures(1, &t); glBindTexture(GL_TEXTURE_BUFFER, t);
    glTexBuffer(GL_TEXTURE_BUFFER, format, b); glBindTexture(GL_TEXTURE_BUFFER, 0);
}
void release(SampleJob& j) {
    if (j.fence) {
        if (const auto destroy = queryProcedure<PFNGLDELETESYNCPROC>("glDeleteSync")) {
            destroy(j.fence); j.fence = nullptr;
        }
    }
    GLuint buffers[]{j.positions, j.layer, j.binding, j.depth, j.coverage, j.output};
    bool has = false; for (auto b : buffers) has = has || b != 0;
    if (has) if (const auto destroy = queryProcedure<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers")) destroy(6, buffers);
    GLuint textures[]{j.layerTexture, j.bindingTexture, j.depthTexture, j.coverageTexture};
    has = false; for (auto t : textures) has = has || t != 0;
    if (has) if (const auto destroy = queryProcedure<void (APIENTRY*)(GLsizei, const GLuint*)>("glDeleteTextures"))
        destroy(4, textures);
    if (j.vao) if (const auto destroy = queryProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays"))
        destroy(1, &j.vao);
    j = {};
}
bool cancelSample(std::uint32_t id) {
    const auto jobCount = sampleJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) if (sampleJobs[i].id == id) {
        auto retainedJobs = sampleJobs;
        release(retainedJobs[i]); removeJob(sampleJobs, i); return true;
    }
    return false;
}
}
void clearPendingComposeJobs() { composeJobs = {}; }
}

bool ngSubmitDepthDrawGpuLayerSample(DepthDrawGpuLayerSamplePacket& packet,
    std::uint32_t& id, GpuError& error) {
    id = 0; error = {}; SampleJob job; bool queued = false;
    try {
        enforce(ngDepthDrawGpuLayerSampleSupported(),
            [] {
                const auto diagnostic = ngDepthDrawGpuLayerSampleSupportDiagnostic();
                return allocateGpuMemory([&] {
                    return "DepthDraw GPU layer sampling requires OpenGL transform feedback support: " +
                        std::string(diagnostic.view());
                });
            }, 780);
        validateDepthDrawGpuLayerSamplePacket(packet);
        enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(packet.layer.convolution),
            "DepthDraw GPU layer sampling does not support custom median convolution yet", 784);
        ensureProgram();
        GLint oldProgram = 0, oldVao = 0, oldArray = 0, oldTextures[4]{}, oldActive = 0, oldTexture = 0, oldFeedback = 0;
        const auto discarded = glIsEnabled(GL_RASTERIZER_DISCARD);
        glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArray); glGetIntegerv(textureBufferBinding, &oldTexture);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
        for (unsigned i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i); glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &oldTextures[i]);
        }
        glActiveTexture(static_cast<GLenum>(oldActive));
        glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &oldFeedback);
        const auto feedback0 = captureBinding(0), feedback1 = captureBinding(1);
        const auto restore = [&] {
            restoreBinding(0, feedback0); restoreBinding(1, feedback1);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(oldFeedback));
            glBindBuffer(GL_TEXTURE_BUFFER, static_cast<GLuint>(oldTexture));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldArray));
            glBindVertexArray(static_cast<GLuint>(oldVao)); glUseProgram(static_cast<GLuint>(oldProgram));
            for (unsigned i = 0; i < 4; ++i) {
                glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_BUFFER, static_cast<GLuint>(oldTextures[i]));
            }
            glActiveTexture(static_cast<GLenum>(oldActive));
            if (discarded == GL_TRUE) glEnable(GL_RASTERIZER_DISCARD); else glDisable(GL_RASTERIZER_DISCARD);
        };
        compat::ngRunWithScopeExit([&] {
            auto upload = ngBuildDepthDrawGpuLayerSampleUpload(packet);
            job.id = nextSampleId++; job.layerIndex = packet.layerIndex; job.count = packet.documentPositions.size();
            glGenVertexArrays(1, &job.vao); glBindVertexArray(job.vao);
            job.positions = buffer(GL_ARRAY_BUFFER, upload.documentPositions);
            texture(job.layer, job.layerTexture, upload.layer, GL_R32F);
            texture(job.binding, job.bindingTexture, upload.binding, GL_R32F);
            texture(job.depth, job.depthTexture, upload.depthPixels, GL_R8UI);
            std::array<std::uint8_t, 4> emptyCoverage{};
            // glBufferData consumes these bytes before this automatic buffer expires.
            const auto coveragePixels = upload.normalCoveragePixels.empty()
                ? std::span<const std::uint8_t>(emptyCoverage) : upload.normalCoveragePixels.view();
            texture(job.coverage, job.coverageTexture, coveragePixels, GL_R8UI);
            const auto count = packet.documentPositions.size();
            const auto bytes = static_cast<GLsizeiptr>(count * sizeof(float));
            glGenBuffers(1, &job.output); glBindBuffer(GL_ARRAY_BUFFER, job.output);
            glBufferData(GL_ARRAY_BUFFER, bytes * 2, nullptr, GL_STREAM_READ);
            glUseProgram(program); glUniform1i(layerUniform, 0); glUniform1i(bindingUniform, 1);
            glUniform1i(depthUniform, 2); glUniform1i(coverageUniform, 3);
            GLuint textures[]{job.layerTexture, job.bindingTexture, job.depthTexture, job.coverageTexture};
            for (unsigned i = 0; i < 4; ++i) {
                glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_BUFFER, textures[i]);
            }
            glEnableVertexAttribArray(0); glBindBuffer(GL_ARRAY_BUFFER, job.positions);
            glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE,
                static_cast<GLsizei>(DepthDrawGpuDocumentPositionStride * sizeof(float)), nullptr);
            glEnableVertexAttribArray(1); glBindBuffer(GL_ARRAY_BUFFER, job.positions);
            glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE,
                static_cast<GLsizei>(DepthDrawGpuDocumentPositionStride * sizeof(float)),
                reinterpret_cast<void*>(static_cast<GLintptr>(sizeof(float))));
            glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, job.output, 0, bytes);
            glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, job.output, bytes, bytes);
            glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_POINTS);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count)); glEndTransformFeedback();
            if (discarded == GL_FALSE) glDisable(GL_RASTERIZER_DISCARD);
            job.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); glFlush();
            glDisableVertexAttribArray(0); glDisableVertexAttribArray(1);
            for (auto unit : {GL_TEXTURE3, GL_TEXTURE2, GL_TEXTURE1, GL_TEXTURE0}) {
                glActiveTexture(unit); glBindTexture(GL_TEXTURE_BUFFER, 0);
            }
            sampleJobs.append(job); queued = true; id = job.id;
        }, restore);
        return true;
    } catch (const compat::ThrowableException& e) {
        if (!queued) release(job); error = e.msg; return false;
    }
}
namespace gpu_detail {
namespace {
bool pollSample(std::uint32_t id, std::uint64_t timeout, DepthDrawGpuLayerSamplePollResult& r, GpuError& error) {
    r = {}; error = {};
    const auto jobCount = sampleJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) {
        if (sampleJobs[i].id != id) continue;
        const auto flags = static_cast<GLbitfield>(timeout > 0 ? GL_SYNC_FLUSH_COMMANDS_BIT : 0);
        const auto status = glClientWaitSync(sampleJobs[i].fence, flags, timeout);
        if (status == GL_TIMEOUT_EXPIRED) return true;
        if (status == GL_WAIT_FAILED) {
            error = GpuError::fromStaticLiteral("DepthDraw GPU layer sample fence wait failed");
            auto retainedJobs = sampleJobs;
            release(retainedJobs[i]); removeJob(sampleJobs, i); return false;
        }
        compat::Slice<float> valid, depths;
        valid.resize(sampleJobs[i].count, std::numeric_limits<float>::quiet_NaN());
        depths.resize(sampleJobs[i].count, std::numeric_limits<float>::quiet_NaN());
        const auto bytes = static_cast<GLsizeiptr>(sampleJobs[i].count * sizeof(float));
        const auto offset = static_cast<GLintptr>(sampleJobs[i].count * sizeof(float));
        GLint old = 0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old);
        glBindBuffer(GL_ARRAY_BUFFER, sampleJobs[i].output);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, bytes, valid.data());
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, bytes, depths.data());
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(old));
        r.ready = true; r.readback.layerIndex = sampleJobs[i].layerIndex;
        r.readback.validSamples.resize(valid.size());
        for (std::size_t v = 0; v < valid.size(); ++v) r.readback.validSamples[v] = valid[v] > 0.5f ? 1 : 0;
        r.readback.sampleDepths = depths;
        auto retainedJobs = sampleJobs;
        release(retainedJobs[i]); removeJob(sampleJobs, i); return true;
    }
    error = GpuError::fromStaticLiteral("DepthDraw GPU layer sample job was not found"); return false;
}
}
bool submitDepthDrawGpuComposeBackend(DepthDrawGpuComposePacket& packet, std::uint32_t& id, GpuError& error) {
    id = 0; error = {}; ComposeJob job;
    try {
        enforce(ngDepthDrawGpuLayerSampleSupported(),
            [] {
                const auto diagnostic = ngDepthDrawGpuLayerSampleSupportDiagnostic();
                return allocateGpuMemory([&] {
                    return "DepthDraw GPU composition requires OpenGL layer sampling support: " +
                        std::string(diagnostic.view());
                });
            }, 987);
        for (auto layer : packet.layers) enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(layer.convolution),
            "DepthDraw GPU composition does not support custom median convolution yet", 991);
        job.id = nextComposeId++; job.ids.resize(packet.layers.size());
        job.readbacks.resize(packet.layers.size()); job.ready.resize(packet.layers.size());
        const auto layerCount = packet.layers.size();
        for (std::size_t i = 0; i < layerCount; ++i) {
            auto sample = ngBuildDepthDrawGpuLayerSamplePacket(packet, i);
            std::uint32_t sampleId = 0;
            if (!ngSubmitDepthDrawGpuLayerSample(sample, sampleId, error)) {
                for (auto submitted : job.ids) if (submitted) cancelSample(submitted);
                return false;
            }
            job.ids[i] = sampleId;
        }
        composeJobs.append(job); id = job.id; return true;
    } catch (const compat::ThrowableException& e) {
        for (auto submitted : job.ids) if (submitted) cancelSample(submitted);
        error = e.msg; return false;
    }
}
bool pollDepthDrawGpuComposeBackend(std::uint32_t id, const DepthDrawGpuComposePacket& packet,
    DepthDrawGpuDispatchPollResult& r, GpuError& error) {
    r = {}; error = {};
    const auto at = []<class T>(compat::Slice<T>& values, std::size_t index,
        std::size_t line) -> T& {
        if (index >= values.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), line);
        return values[index];
    };
    const auto jobCount = composeJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) {
        if (at(composeJobs, i, 1030).id != id) continue;
        const auto ids = at(composeJobs, i, 1031).ids;
        const auto cancelRemaining = [&](std::size_t sourceLine) {
            const auto remaining = at(composeJobs, i, sourceLine).ids;
            for (std::size_t j = 0; j < remaining.size(); ++j)
                if (!at(at(composeJobs, i, sourceLine + 1).ready, j, sourceLine + 1))
                    cancelSample(remaining[j]);
            removeJob(composeJobs, i, sourceLine + 5);
        };
        for (std::size_t layer = 0; layer < ids.size(); ++layer) {
            if (at(at(composeJobs, i, 1032).ready, layer, 1032)) continue;
            DepthDrawGpuLayerSamplePollResult polled;
            if (!ngPollDepthDrawGpuLayerSample(ids[layer], polled, error)) {
                cancelRemaining(1035); return false;
            }
            if (!polled.ready) return true;
            if (polled.readback.layerIndex != layer) {
                error = GpuError::fromStaticLiteral("DepthDraw GPU compose layer readback index mismatch");
                cancelRemaining(1046); return false;
            }
            at(at(composeJobs, i, 1054).readbacks, layer, 1054) = polled.readback;
            at(at(composeJobs, i, 1055).ready, layer, 1055) = true;
        }
        r.ready = true;
        r.readback = ngBuildDepthDrawGpuComposeReadback(packet, at(composeJobs, i, 1058).readbacks);
        removeJob(composeJobs, i, 1059); return true;
    }
    error = GpuError::fromStaticLiteral("DepthDraw GPU compose job was not found"); return false;
}
void cancelDepthDrawGpuComposeBackend(std::uint32_t id) {
    const auto jobCount = composeJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) if (composeJobs[i].id == id) {
        const auto ids = composeJobs[i].ids;
        for (std::size_t j = 0; j < ids.size(); ++j) if (!composeJobs[i].ready[j]) cancelSample(ids[j]);
        removeJob(composeJobs, i); return;
    }
}
}
bool ngPollDepthDrawGpuLayerSample(std::uint32_t id, DepthDrawGpuLayerSamplePollResult& r, GpuError& error) {
    return pollSample(id, 0, r, error);
}
std::size_t ngPendingDepthDrawGpuLayerSampleJobCount() { return sampleJobs.size(); }
}
