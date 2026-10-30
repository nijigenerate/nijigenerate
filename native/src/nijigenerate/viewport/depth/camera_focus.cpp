// Copyright 2024, nijigenerate Project. Distributed under the 2-Clause BSD License.
#include "camera.hpp"
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <runtime_state.hpp>
#include <algorithm>
#include <nijigenerate/compat/array_range_error.hpp>

namespace nijigenerate::viewport::depth {
void focusGridOrigin(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid) {
    if (!grid) return;
    int width = 0, height = 0;
    nicxlive::core::inGetViewport(width, height);
    float largestBounds = 1.0f;
    if (!grid->getVertices().empty()) {
        const auto first = [&] {
            const auto firstVertices = grid->getVertices();
            if (firstVertices.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, firstVertices.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/camera.d"), 75);
            return firstVertices[0].toVector();
        }();
        nicxlive::core::math::Vec2 minimum{first.x, first.y};
        auto maximum = minimum;
        const auto vertices = grid->getVertices();
        const auto count = vertices.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto vertex = vertices[i].toVector();
            minimum.x = std::min(minimum.x, vertex.x);
            minimum.y = std::min(minimum.y, vertex.y);
            maximum.x = std::max(maximum.x, vertex.x);
            maximum.y = std::max(maximum.y, vertex.y);
        }
        largestBounds = std::max(1.0f, std::max(maximum.x - minimum.x, maximum.y - minimum.y));
    }
    const float largestViewport = std::max(1.0f, static_cast<float>(std::min(width, height)));
    const float factor = largestViewport / largestBounds;
    if (auto active = project::incActiveProject())
        active->CameraFocused.emit(std::clamp(factor * .90f, .1f, 2.5f),
            nicxlive::core::math::Vec2{0, 0});
}
} // namespace nijigenerate::viewport::depth
