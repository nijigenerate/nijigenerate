#include "operation.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"

#include <array>

namespace nijigenerate::viewport::depth::tools::operation {

std::string DepthAttachedPointOperation::label() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("attached point %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(index)};
    return core::translation::ngFormatTyped(pattern.view(), arguments);
}

std::string DepthRingOperation::label() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("ring %.0f,%.0f"));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(p0.x), core::translation::ngFormatArgument(p0.y)};
    return core::translation::ngFormatTyped(pattern.view(), arguments);
}

std::string DepthPlaneOperation::label() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("plane %.0f,%.0f"));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(center.x), core::translation::ngFormatArgument(center.y)};
    return core::translation::ngFormatTyped(pattern.view(), arguments);
}

} // namespace nijigenerate::viewport::depth::tools::operation
