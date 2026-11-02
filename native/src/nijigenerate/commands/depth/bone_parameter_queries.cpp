#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_string_concatenation.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <charconv>
#include <new>
#include <stdexcept>
#include <string>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::param::Parameter;

bool isLiveDepthRigRoot(std::shared_ptr<ExDepthRigRoot> root) {
    if (!root) return false;
    for (auto liveRoot : depthBoneRoots()) {
        if (liveRoot == root) return true;
    }
    return false;
}

compat::RetainedStringSlice keypointKey(compat::Vec2u keypoint) {
    try {
        char bytes[10];
        const auto xEnd = std::to_chars(bytes, bytes + sizeof(bytes), keypoint.x).ptr;
        const auto x = compat::RetainedString(std::string(bytes, xEnd));
        const auto yEnd = std::to_chars(bytes, bytes + sizeof(bytes), keypoint.y).ptr;
        const auto y = compat::RetainedString(std::string(bytes, yEnd));
        return concatenateBoneStrings({x.view(), ":", y.view()});
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

compat::Slice<compat::Vec2u> depthBoneKeypoints(std::shared_ptr<Parameter> parameter) {
    compat::Slice<compat::Vec2u> result;
    if (!parameter) return result;
    // Original axisPointCount returns uint32, and foreach ranges capture their
    // bounds. The inner range is constructed once for every outer coordinate.
    const auto xCount = static_cast<std::uint32_t>(parameter->axisPointCount(0));
    for (std::uint32_t x = 0; x < xCount; ++x) {
        const auto yCount = static_cast<std::uint32_t>(parameter->axisPointCount(1));
        for (std::uint32_t y = 0; y < yCount; ++y) result.append(compat::Vec2u{x, y});
    }
    if (result.empty()) {
        // The original no-argument overload delegates using its current value.
        const auto closest = parameter->findClosestKeypoint();
        result.append(compat::Vec2u{closest.x, closest.y});
    }
    return result;
}

} // namespace nijigenerate::commands::depth::bone::detail
