#include "bone_key_projection.hpp"
#include "detail/bone_runtime.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::commands::depth::bone {
namespace {
[[noreturn]] void failProjection(std::string_view message, std::size_t line) {
    compat::RetainedString copiedMessage;
    try {
        copiedMessage = compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(copiedMessage,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), line);
}
}

compat::Slice<double> ngDepthBoneKeyProjection(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        std::uint64_t boneUuid, std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        compat::Vec2u key) {
    if (!core::thread_context::ngIsMainThreadOrUnattached()) {
        failProjection("Depth projection requires the editor thread", 1105);
    }
    auto runtime = detail::buildDepthRigRuntime(root, parameter, key);
    auto bone = runtime.find(boneUuid);
    if (!bone) {
        failProjection("Depth projection bone is missing", 1108);
    }
    const auto matrix = (*bone)->skinMatrix;
    constexpr std::array<std::array<float, 4>, 4> basis{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    }};
    compat::Slice<double> result;
    for (const auto& point : basis) {
        std::array<float, 4> projected{};
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                const float product = nicxlive::core::math::detail::inmathProduct(
                    matrix[row][column], point[column]);
                projected[row] = nicxlive::core::math::detail::inmathAccumulate(projected[row], product);
            }
        }
        result.append(static_cast<double>(projected[0]));
        result.append(static_cast<double>(projected[1]));
    }
    return result;
}

} // namespace nijigenerate::commands::depth::bone
