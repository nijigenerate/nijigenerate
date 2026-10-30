#pragma once
#include "gpu.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <concepts>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
namespace nijigenerate::viewport::depth::draw::gpu_detail {
// Capability queries represent absent procedures as null, like the original
// BindBC pointers. Keep the real backend's current-context policy.
inline SDL_FunctionPointer queryProcedure(const char* name) {
    try { return platform::ngGLGetBackendProcedure(name); }
    catch (const std::logic_error&) { return nullptr; }
    catch (const std::runtime_error&) { return nullptr; }
}
template<class Function> Function queryProcedure(const char* name) {
    static_assert(sizeof(Function) == sizeof(SDL_FunctionPointer));
    return std::bit_cast<Function>(queryProcedure(name));
}
// Call only with pure byte/buffer work, after any provider or GL calls finish.
template<class F> decltype(auto) allocateGpuMemory(F&& allocate) {
    try { return allocate(); }
    catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
    catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
}
inline compat::RetainedString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return compat::RetainedString::fromStaticLiteral("Enforcement failed");
    // Phobos bailOut copies even a nonempty literal with msg.idup.
    return allocateGpuMemory([&] { return compat::RetainedString(std::string(message)); });
}
inline void enforce(bool value, std::string message,
    std::source_location source = std::source_location::current()) {
    if (!value) throw compat::ThrowableException(copyEnforceMessage(message), {}, source);
}
template<std::size_t N> void enforce(bool value, const char (&message)[N],
    std::source_location source = std::source_location::current()) {
    if (!value) throw compat::ThrowableException(
        copyEnforceMessage(std::string_view(message, N - 1)), {}, source);
}
template<class F> requires std::invocable<F&>
void enforce(bool value, F&& message, std::source_location source = std::source_location::current()) {
    if (!value) enforce(false, message(), source);
}
inline void enforce(bool value, std::string message, std::size_t sourceLine) {
    if (!value) throw compat::ThrowableException(copyEnforceMessage(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), sourceLine);
}
template<std::size_t N> void enforce(bool value, const char (&message)[N], std::size_t sourceLine) {
    if (!value) throw compat::ThrowableException(copyEnforceMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), sourceLine);
}
template<class F> requires std::invocable<F&>
void enforce(bool value, F&& message, std::size_t sourceLine) {
    if (!value) enforce(false, message(), sourceLine);
}
bool isValidDepthDrawGpuChannel(int);
bool isValidDepthDrawGpuConvolution(int);
void validateDepthDrawGpuPacket(const DepthDrawGpuComposePacket&);
void validateDepthDrawGpuLayerSamplePacket(const DepthDrawGpuLayerSamplePacket&);
void validateDepthDrawGpuReadback(const DepthDrawGpuComposePacket&, const DepthDrawGpuComposeReadback&);
DepthDrawGpuComposePacket cloneDepthDrawGpuPacket(const DepthDrawGpuComposePacket&);
bool submitDepthDrawGpuComposeBackend(DepthDrawGpuComposePacket&, std::uint32_t&, GpuError&);
bool pollDepthDrawGpuComposeBackend(std::uint32_t, const DepthDrawGpuComposePacket&,
    DepthDrawGpuDispatchPollResult&, GpuError&);
void cancelDepthDrawGpuComposeBackend(std::uint32_t);
void clearPendingComposeJobs();
}
