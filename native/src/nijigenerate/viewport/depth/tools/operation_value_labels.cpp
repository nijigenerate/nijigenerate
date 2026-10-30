#include "nijigenerate/viewport/depth/tools/operation.hpp"
#include "nijigenerate/viewport/depth/tools/detail/fixed_value_format.hpp"

namespace nijigenerate::viewport::depth::tools::operation {

std::string DepthAttachedPointOperation::valueLabel() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    return detail::formatDepthValue(amount);
}

std::string DepthRingOperation::valueLabel() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    return detail::formatDepthValue(amount);
}

std::string DepthPlaneOperation::valueLabel() {
    [[maybe_unused]] const auto receiverLifetime = retainReceiver();
    return detail::formatDepthValue(targetDepth);
}

} // namespace nijigenerate::viewport::depth::tools::operation
