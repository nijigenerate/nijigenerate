#include "layer.hpp"

namespace nijigenerate::viewport::depth::draw {
bool DepthDrawLayer::hasDepthPixels() const {
    return width > 0 && height > 0 && depthPixels.size() >=
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
}
bool DepthDrawLayer::hasNormalCoverage() const {
    return normalCoverage.size() >= static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
}
} // namespace nijigenerate::viewport::depth::draw
