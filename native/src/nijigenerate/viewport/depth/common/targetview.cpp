#include "nijigenerate/viewport/depth/common/targetview.hpp"
#include "nijigenerate/viewport/depth/common/vertex_copy.hpp"
#include "nijigenerate/viewport/depth/common/target_inverse.hpp"
#include "nijigenerate/ext/nodes/exdepthnumeric.hpp"
#include "nijigenerate/ext/nodes/exdepthmapped.hpp"
#include <nodes/deformable.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <common/object_key_policy.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <string>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <source_location>
#include <new>
#include <stdexcept>

namespace nijigenerate::viewport::depth::common::targetview {

float ngDepthDisplayScaleForTargets(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Deformable>> targets) {
    bool hasBounds = false;
    nicxlive::core::math::Vec2 minPoint{}, maxPoint{};
    for (auto target : targets) {
        if (!target) continue;
        const auto vertices = target->getVertices();
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto vertex = vertices.get(i);
            if (!hasBounds) { minPoint = maxPoint = vertex; hasBounds = true; }
            else {
                minPoint.x = std::min(minPoint.x, vertex.x); minPoint.y = std::min(minPoint.y, vertex.y);
                maxPoint.x = std::max(maxPoint.x, vertex.x); maxPoint.y = std::max(maxPoint.y, vertex.y);
            }
        }
    }
    return hasBounds ? ngDepthDisplayScaleForBounds(minPoint, maxPoint) : 0.0f;
}

float ngDepthDisplayScaleForTarget(std::shared_ptr<nicxlive::core::nodes::Deformable> target) {
    if (!target || target->getVertices().empty()) return 1.0f;
    const auto first = [&] {
        const auto firstVertices = target->getVertices();
        if (firstVertices.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, firstVertices.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/common/targetview.d"), 74);
        return firstVertices.get(0);
    }();
    auto minPoint = first, maxPoint = minPoint;
    const auto vertices = target->getVertices();
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto vertex = vertices.get(i);
        minPoint.x = std::min(minPoint.x, vertex.x); minPoint.y = std::min(minPoint.y, vertex.y);
        maxPoint.x = std::max(maxPoint.x, vertex.x); maxPoint.y = std::max(maxPoint.y, vertex.y);
    }
    return ngDepthDisplayScaleForBounds(minPoint, maxPoint);
}

float ngDepthDisplayScaleForTargetsInNodeSpace(std::shared_ptr<nicxlive::core::nodes::Node> root,
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Deformable>> targets) {
    using nicxlive::core::math::Mat4;
    bool hasBounds = false;
    nicxlive::core::math::Vec2 minPoint{}, maxPoint{};
    const auto rootInverse = root ? detail::inverseForDepthTargets(root->transform().matrix()) : Mat4::identity();
    for (auto target : targets) {
        const auto node = std::dynamic_pointer_cast<nicxlive::core::nodes::Node>(target);
        if (!target || !node) continue;
        const auto targetToRoot = rootInverse * node->transform().matrix();
        const auto vertices = target->getVertices();
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto vertex = vertices.get(i);
            // Preserve the actual inmath vec4 fold, including zero Z and unit W.
            auto component = [&](int row) {
                float result = 0.0f;
                const float values[]{vertex.x,vertex.y,0.0f,1.0f};
                for (int column = 0; column < 4; ++column) {
                    result = nicxlive::core::math::detail::inmathAccumulate(result,
                        nicxlive::core::math::detail::inmathProduct(targetToRoot[row][column],values[column]));
                }
                return result;
            };
            const nicxlive::core::math::Vec2 point{component(0),component(1)};
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
            if (!hasBounds) { minPoint = maxPoint = point; hasBounds = true; }
            else {
                minPoint.x = std::min(minPoint.x, point.x); minPoint.y = std::min(minPoint.y, point.y);
                maxPoint.x = std::max(maxPoint.x, point.x); maxPoint.y = std::max(maxPoint.y, point.y);
            }
        }
    }
    return hasBounds ? ngDepthDisplayScaleForBounds(minPoint, maxPoint) : 0.0f;
}

DepthTargetView::DepthTargetView(Target value) : target(std::move(value)),
    depthMapped(std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(target)) {
    resetFromTarget();
    rebuildBounds();
    rebuildTopology();
}

DepthTargetView::~DepthTargetView() = default;

namespace {
#if defined(_WIN32)
using SourceReal = double;
#else
using SourceReal = long double;
#endif
SourceReal pointDistance(nicxlive::core::math::Vec2 a, nicxlive::core::math::Vec2 b) {
    const float x = a.x - b.x, y = a.y - b.y;
    // Inmath squares float components before accumulating into selected native real.
    SourceReal squared = 0.0;
    squared += static_cast<SourceReal>(x * x);
    squared += static_cast<SourceReal>(y * y);
    return std::sqrt(squared);
}
}

void DepthTargetView::refreshGeometry() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    rebuildBounds();
    rebuildTopology();
    normalizeDepthLength();
}

DepthTargetView::Vec2 DepthTargetView::localVertex(std::size_t index) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto vertices = getVertices();
    return index < vertices.size() ? vertices[index] : Vec2{0.0f, 0.0f};
}

DepthTargetView::Vec2 DepthTargetView::snapLocalPoint(Vec2 point) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto vertices = getVertices();
    if (vertices.empty()) return point;
    auto best = vertices[0];
    auto bestDistance = pointDistance(point, best);
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto distance = pointDistance(point, vertices[i]);
        if (distance < bestDistance) { best = vertices[i]; bestDistance = distance; }
    }
    return best;
}

std::ptrdiff_t DepthTargetView::nearestLocalVertexIndex(Vec2 point) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto vertices = getVertices();
    if (vertices.empty()) return -1;
    std::ptrdiff_t best = 0;
    auto bestDistance = pointDistance(point, vertices[0]);
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto distance = pointDistance(point, vertices[i]);
        if (distance < bestDistance) { best = static_cast<std::ptrdiff_t>(i); bestDistance = distance; }
    }
    return best;
}

std::ptrdiff_t DepthTargetView::nearestProjectedVertex(Vec2 point, float radius) {
    std::ptrdiff_t best = -1;
    auto bestDistance = radius;
    const auto iterationPoints = projectedPoints;
    for (std::size_t i = 0; i < iterationPoints.size(); ++i) {
        const auto distance = pointDistance(iterationPoints[i], point);
        if (distance < bestDistance) {
            best = static_cast<std::ptrdiff_t>(i);
            bestDistance = static_cast<float>(distance);
        }
    }
    return best;
}

void DepthTargetView::replaceWorkingDepths(compat::Slice<float> values) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    depths = values.duplicate();
    ext::nodes::ngNormalizeDepths(std::span<float>{depths.data(), depths.size()});
    normalizeDepthLength();
}

void DepthTargetView::resetWorkingDepths() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    replaceWorkingDepths(baseDepths);
}

void DepthTargetView::clearBaseDepths() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    baseDepths.resize(compat::ngRequireObject(target)->getVertices().size());
    for (auto& value : baseDepths) value = 0.0f;
    replaceWorkingDepths(baseDepths);
}

void DepthTargetView::resetFromTarget() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    depths = depthMapped ? depthMapped->copyDepths() : compat::Slice<float>{};
    normalizeDepthLength();
    baseDepths = depths.duplicate();
}

float DepthTargetView::depthAtLocalPoint(Vec2 point) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto vertices = getVertices();
    if (vertices.empty() || depths.empty()) return 0.0f;
    std::size_t bestIndex = 0;
    auto bestDistance = pointDistance(point, vertices[0]);
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto distance = pointDistance(point, vertices[i]);
        if (distance < bestDistance) { bestIndex = i; bestDistance = distance; }
    }
    return bestIndex < depths.size() ? depths[bestIndex] : 0.0f;
}

void DepthTargetView::rebuildBounds() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto vertices = detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (vertices.empty()) {
        minPoint = {0.0f, 0.0f};
        maxPoint = {1.0f, 1.0f};
        return;
    }
    minPoint = vertices[0];
    maxPoint = vertices[0];
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto vertex = vertices[i];
        minPoint.x = std::min(minPoint.x, vertex.x);
        minPoint.y = std::min(minPoint.y, vertex.y);
        maxPoint.x = std::max(maxPoint.x, vertex.x);
        maxPoint.y = std::max(maxPoint.y, vertex.y);
    }
}

void DepthTargetView::rebuildTopology() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    indices.resize(0);
    const auto vertices = detail::copyVertices(compat::ngRequireObject(target)->getVertices());
    if (vertices.empty()) return;
    compat::Slice<float> xs, ys;
    for (auto vertex : vertices) { xs.append(vertex.x); ys.append(vertex.y); }
    xs = sortedUnique(xs);
    ys = sortedUnique(ys);
    if (xs.size() < 2 || ys.size() < 2 || xs.size() * ys.size() != vertices.size()) return;
    struct GridKeyHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            return static_cast<std::size_t>(value);
        }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, std::uint16_t,
        GridKeyHash, std::equal_to<std::uint64_t>> lookup;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        std::size_t xi = 0, yi = 0;
        for (std::size_t j = 0; j < xs.size(); ++j) if (xs[j] == vertices[i].x) { xi = j; break; }
        for (std::size_t j = 0; j < ys.size(); ++j) if (ys[j] == vertices[i].y) { yi = j; break; }
        lookup[yi * xs.size() + xi] = static_cast<std::uint16_t>(i);
    }
    for (std::size_t y = 0; y < ys.size() - 1; ++y) {
        for (std::size_t x = 0; x < xs.size() - 1; ++x) {
            const auto k0 = y * xs.size() + x, k1 = k0 + 1;
            const auto k2 = (y + 1) * xs.size() + x, k3 = k2 + 1;
            const auto p0 = lookup.find(k0), p1 = lookup.find(k1);
            const auto p2 = lookup.find(k2), p3 = lookup.find(k3);
            if (p0 == lookup.end() || p1 == lookup.end() || p2 == lookup.end() || p3 == lookup.end()) continue;
            indices.append(compat::Slice<std::uint16_t>{
                p0->second, p1->second, p3->second, p0->second, p3->second, p2->second});
        }
    }
}

void DepthTargetView::normalizeDepthLength() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ext::nodes::ngNormalizeDepths(std::span<float>{depths.data(), depths.size()});
    if (depths.isNull() || depths.size() != compat::ngRequireObject(target)->getVertices().size()) {
        const auto oldLength = depths.size();
        depths.resize(compat::ngRequireObject(target)->getVertices().size());
        for (std::size_t i = oldLength; i < depths.size(); ++i) depths[i] = 0.0f;
    }
}

std::shared_ptr<nicxlive::core::nodes::Node> DepthTargetView::targetNode() {
    return std::dynamic_pointer_cast<nicxlive::core::nodes::Node>(target);
}

compat::Slice<nicxlive::core::math::Vec2> DepthTargetView::getVertices() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return detail::copyVertices(compat::ngRequireObject(target)->getVertices());
}

// object.Object defaults (Digital Mars 2000-2011), BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DepthTargetView::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthTargetView),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.depth.common.targetview.DepthTargetView"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString DepthTargetView::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthTargetView::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthTargetView::opEquals(std::shared_ptr<DepthTargetView> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthTargetView::opCmp(std::shared_ptr<DepthTargetView>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDepthTargetViewObjectEquals(std::shared_ptr<DepthTargetView> left,
        std::shared_ptr<DepthTargetView> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

} // namespace nijigenerate::viewport::depth::common::targetview
