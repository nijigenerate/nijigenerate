#include "gpu_internal.hpp"
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <common/resource_key_map.hpp>
#include <functional>

namespace nijigenerate::viewport::depth::draw {
using namespace gpu_detail;
namespace {
thread_local DepthDrawGpuSupportHook supportHook = nullptr;
thread_local DepthDrawGpuSubmitHook submitHook = nullptr;
thread_local DepthDrawGpuPollHook pollHook = nullptr;
struct PacketIdHash {
    std::size_t operator()(std::uint32_t id) const noexcept { return id; }
};
thread_local nicxlive::core::common::ResourceKeyMap<std::uint32_t, DepthDrawGpuComposePacket,
    PacketIdHash, std::equal_to<std::uint32_t>> pendingPackets;
GpuError diagnostic(compat::Slice<compat::RetainedStringSlice> names) {
    return allocateGpuMemory([&]() -> GpuError {
        std::string text;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (i) text += ", ";
            if (!names[i].empty()) text.append(names[i].view());
        }
        if (text.empty()) return {};
        return GpuError(std::move(text));
    });
}
GpuError unavailableMessage(const GpuError& diagnostic) {
    return allocateGpuMemory([&] {
        return GpuError("DepthDraw GPU composition is unavailable: " + std::string(diagnostic.view()));
    });
}
compat::Slice<compat::RetainedStringSlice> missing(bool layerSample) {
    compat::Slice<compat::RetainedStringSlice> result;
#if defined(NIJIGENERATE_DEPTH_GPU_RENDER)
    const auto check = [&](const auto& name) {
        if (!queryProcedure(name)) result.append(compat::RetainedString::fromStaticLiteral(name));
    };
    check("glCreateShader");
    if (layerSample) { check("glShaderSource"); check("glCompileShader"); check("glCreateProgram"); }
    check("glTransformFeedbackVaryings");
    check("glBeginTransformFeedback");
    check("glEndTransformFeedback");
    check("glBindBufferRange");
    check("glBindBufferBase");
    check("glGetIntegeri_v");
    check("glGetInteger64i_v");
    check("glGetBufferSubData");
    check("glGenVertexArrays");
    check("glBindVertexArray");
    check("glDeleteVertexArrays");
    check("glTexBuffer");
    check("glFenceSync");
    check("glClientWaitSync");
    check("glDeleteSync");
#else
    (void)layerSample;
    result.append(compat::RetainedString::fromStaticLiteral("rendering backend"));
#endif
    return result;
}
}
void ngSetDepthDrawGpuTestHooks(DepthDrawGpuSupportHook support, DepthDrawGpuSubmitHook submit, DepthDrawGpuPollHook poll) {
    supportHook = support; submitHook = submit; pollHook = poll;
}
void ngClearDepthDrawGpuTestHooks() {
    supportHook = nullptr; submitHook = nullptr; pollHook = nullptr; pendingPackets = {};
#if defined(NIJIGENERATE_DEPTH_GPU_RENDER)
    clearPendingComposeJobs();
#endif
}
bool ngDepthDrawGpuSupported() { return supportHook ? supportHook() : ngDepthDrawGpuMissingRequirements().empty(); }
compat::Slice<compat::RetainedStringSlice> ngDepthDrawGpuMissingRequirements() { return missing(false); }
GpuError ngDepthDrawGpuSupportDiagnostic() {
    return ngDepthDrawGpuSupported() ? GpuError::fromStaticLiteral("available")
        : diagnostic(ngDepthDrawGpuMissingRequirements());
}
compat::Slice<compat::RetainedStringSlice> ngDepthDrawGpuLayerSampleMissingRequirements() { return missing(true); }
bool ngDepthDrawGpuLayerSampleSupported() { return ngDepthDrawGpuLayerSampleMissingRequirements().empty(); }
bool ngDepthDrawGpuLayerSampleSupportsConvolution(std::int32_t value) {
    return isValidDepthDrawGpuConvolution(value) &&
        static_cast<io::depthimage::DepthImageConvolution>(value) != io::depthimage::DepthImageConvolution::MedianCustom;
}
GpuError ngDepthDrawGpuLayerSampleSupportDiagnostic() {
    return ngDepthDrawGpuLayerSampleSupported() ? GpuError::fromStaticLiteral("available")
        : diagnostic(ngDepthDrawGpuLayerSampleMissingRequirements());
}
bool ngSubmitDepthDrawGpuCompose(DepthDrawGpuComposePacket& p, std::uint32_t& id, GpuError& error) {
    id = 0; error = {};
    try {
        validateDepthDrawGpuPacket(p);
        for (auto layer : p.layers) enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(layer.convolution),
            "DepthDraw GPU composition does not support custom median convolution yet", 1109);
    } catch (const compat::ThrowableException& e) { error = e.msg; return false; }
    if (submitHook) {
        const bool submitted = submitHook(p, id, error);
        if (submitted) pendingPackets[id] = cloneDepthDrawGpuPacket(p);
        return submitted;
    }
#if defined(NIJIGENERATE_DEPTH_GPU_RENDER)
    const bool submitted = submitDepthDrawGpuComposeBackend(p, id, error);
    if (submitted) pendingPackets[id] = cloneDepthDrawGpuPacket(p);
    return submitted;
#else
    const auto diagnostic = ngDepthDrawGpuSupportDiagnostic();
    error = unavailableMessage(diagnostic);
    return false;
#endif
}
bool ngPollDepthDrawGpuCompose(std::uint32_t id, DepthDrawGpuDispatchPollResult& result, GpuError& error) {
    result = {}; error = {};
    if (pollHook) {
        const bool polled = pollHook(id, result, error);
        if (!polled || !result.ready) return polled;
        const auto found = pendingPackets.find(id);
        if (found == pendingPackets.end()) {
            error = GpuError::fromStaticLiteral("DepthDraw GPU readback has no pending packet"); return false;
        }
        // Dereference once: the real iterator retains this exact Entry even if
        // a callback removes/replaces its map bucket while validation runs.
        const auto& packet = found->second;
        try { validateDepthDrawGpuReadback(packet, result.readback); }
        catch (const compat::ThrowableException& e) {
            pendingPackets.erase(id); error = e.msg; return false;
        }
        pendingPackets.erase(id); return true;
    }
#if defined(NIJIGENERATE_DEPTH_GPU_RENDER)
    const auto found = pendingPackets.find(id);
    if (found == pendingPackets.end()) {
        error = GpuError::fromStaticLiteral("DepthDraw GPU readback has no pending packet"); return false;
    }
    const auto& packet = found->second;
    const bool polled = pollDepthDrawGpuComposeBackend(id, packet, result, error);
    if (!polled || !result.ready) return polled;
    try { validateDepthDrawGpuReadback(packet, result.readback); }
    catch (const compat::ThrowableException& e) {
        pendingPackets.erase(id); error = e.msg; return false;
    }
    pendingPackets.erase(id); return true;
#else
    const auto diagnostic = ngDepthDrawGpuSupportDiagnostic();
    error = unavailableMessage(diagnostic);
    return false;
#endif
}
void ngCancelDepthDrawGpuCompose(std::uint32_t id) {
    if (!id) return;
#if defined(NIJIGENERATE_DEPTH_GPU_RENDER)
    if (!submitHook) cancelDepthDrawGpuComposeBackend(id);
#endif
    pendingPackets.erase(id);
}
std::size_t ngPendingDepthDrawGpuComposeJobCount() { return pendingPackets.size(); }
bool ngSubmitDepthDrawGpuTargetCompose(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target, std::int32_t width, std::int32_t height,
    DepthDrawGpuTargetComposeJob& job, GpuError& error) {
    job = {}; error = {};
    auto packet = ngBuildDepthDrawGpuComposePacket(session, target, width, height);
    std::uint32_t id = 0;
    if (!ngSubmitDepthDrawGpuCompose(packet, id, error)) return false;
    job.jobId = id; job.packet = cloneDepthDrawGpuPacket(packet); return true;
}
bool ngPollDepthDrawGpuTargetCompose(DepthDrawGpuTargetComposeJob& job,
    DepthDrawGpuTargetComposePollResult& result, GpuError& error) {
    result = {}; error = {};
    DepthDrawGpuDispatchPollResult dispatch;
    if (!ngPollDepthDrawGpuCompose(job.jobId, dispatch, error)) return false;
    if (!dispatch.ready) return true;
    result.ready = true; result.readback = dispatch.readback;
    result.result = ngDepthDrawComposeResultFromGpuReadback(job.packet, dispatch.readback); return true;
}
void ngCancelDepthDrawGpuTargetCompose(DepthDrawGpuTargetComposeJob& job) {
    ngCancelDepthDrawGpuCompose(job.jobId); job = {};
}
#if !defined(NIJIGENERATE_DEPTH_GPU_RENDER)
bool ngSubmitDepthDrawGpuLayerSample(DepthDrawGpuLayerSamplePacket&, std::uint32_t& id, GpuError& error) {
    id = 0; error = GpuError::fromStaticLiteral("DepthDraw GPU layer sampling requires the rendering backend");
    return false;
}
bool ngPollDepthDrawGpuLayerSample(std::uint32_t, DepthDrawGpuLayerSamplePollResult& r, GpuError& error) {
    r = {}; error = GpuError::fromStaticLiteral("DepthDraw GPU layer sampling requires the rendering backend");
    return false;
}
std::size_t ngPendingDepthDrawGpuLayerSampleJobCount() { return 0; }
#endif
}
