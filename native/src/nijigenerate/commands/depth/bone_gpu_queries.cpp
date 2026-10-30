#include "bone_gpu_queries.hpp"
#include "bone_gpu_async.hpp"
#include "detail/bone_flush.hpp"
#include "detail/bone_input.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::commands::depth::bone {

bool ngDepthBoneGpuSupported() {
    return bone_gpu_async::ngDepthBoneGpuAsyncSupported();
}

compat::RetainedString ngDepthBoneGpuSupportDiagnostic() try {
    if (bone_gpu_async::ngDepthBoneGpuAsyncSupported())
        return compat::RetainedString::fromStaticLiteral("Depth bone GPU async deformation is supported");
    auto missing = bone_gpu_async::ngDepthBoneGpuAsyncMissingRequirements();
    std::string message = "Depth bone GPU async deformation is unavailable; missing: ";
    bool first = true;
    for (auto name : missing) {
        if (!first) message.append(", ");
        first = false;
        if (!name.empty()) message.append(name.data(), name.size());
    }
    return compat::RetainedString(std::move(message));
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
}

nicxlive::core::math::Vec2Array ngDepthBoneGpuReadbackToOffsets(
        compat::Slice<float> xs, compat::Slice<float> ys) {
    if (xs.size() != ys.size())
        detail::failInput("Depth bone GPU readback arrays must have the same length", 1166);
    nicxlive::core::math::Vec2Array offsets;
    offsets.setLength(xs.size());
    const auto count = xs.size();
    for (std::size_t i = 0; i < count; ++i)
        offsets.set(i, nicxlive::core::math::Vec2{xs[i], ys[i]});
    return offsets;
}

namespace detail {

void enforceDepthBoneGpuAvailable(compat::RetainedStringSlice context) {
    if (ngDepthBoneGpuSupported()) return;
    std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(context),
        core::translation::ngFormatArgument(ngDepthBoneGpuSupportDiagnostic())};
    auto message = compat::RetainedString(core::translation::ngFormatTyped(
        "Depth Bone GPU unavailable for %s: %s", arguments));
    writeDepthBoneGpuFatalLog(message);
    failInput(message.view(), 1162);
}

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
