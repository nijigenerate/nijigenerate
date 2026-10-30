#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/viewport/depth/camera.hpp"
#include "nijigenerate/viewport/depth/detail/float_sort.hpp"
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::nodes { class Deformable; class Node; }
namespace nijigenerate::ext::nodes { class DepthMappedNode; }

namespace nijigenerate::viewport::depth::common::targetview {

inline constexpr float DepthTargetDisplayPlaneSize = 2.9f;
inline constexpr float DepthTargetDisplayZScale = 0.42f;

inline float ngDepthDisplayScaleForBounds(nicxlive::core::math::Vec2 minPoint,
    nicxlive::core::math::Vec2 maxPoint) {
    const nicxlive::core::math::Vec2 size{maxPoint.x - minPoint.x, maxPoint.y - minPoint.y};
    // Preserve the observed D manifest-ratio rounding at this call site.
    const float displayRatio = static_cast<float>(0.42 / 2.9);
    return std::max(1.0f, std::max(size.x, size.y) * displayRatio);
}

inline float ngDepthTargetRoundDepth(float value) {
    return std::round(value * 1000.0f) / 1000.0f;
}

inline float ngDepthDisplayScaleForDocument(std::int32_t width, std::int32_t height) {
    return ngDepthDisplayScaleForBounds({0.0f, 0.0f}, {static_cast<float>(width), static_cast<float>(height)});
}

inline float ngDepthTargetClampDepth(float value) {
    return std::isfinite(value) ? ngDepthTargetRoundDepth(value) : 0.0f;
}

float ngDepthDisplayScaleForTargets(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Deformable>> targets);
float ngDepthDisplayScaleForTargetsInNodeSpace(std::shared_ptr<nicxlive::core::nodes::Node> root,
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Deformable>> targets);
float ngDepthDisplayScaleForTarget(std::shared_ptr<nicxlive::core::nodes::Deformable> target);

// Retained target owner; runtime-dependent bodies are defined in targetview.cpp.
class DepthTargetView : public std::enable_shared_from_this<DepthTargetView> {
    using Vec2 = nicxlive::core::math::Vec2;
    using Target = std::shared_ptr<nicxlive::core::nodes::Deformable>;
    Target target;
    std::shared_ptr<ext::nodes::DepthMappedNode> depthMapped;
    Vec2 minPoint{0.0f, 0.0f};
    Vec2 maxPoint{1.0f, 1.0f};
    compat::Slice<std::uint16_t> indices;
    float sharedDepthDisplayScale{std::numeric_limits<float>::quiet_NaN()};

    compat::Slice<float> sortedUnique(compat::Slice<float> values) {
        return nijigenerate::viewport::depth::detail::sortedUniqueFloats(values);
    }
    void rebuildBounds();
    void rebuildTopology();
public:
    compat::Slice<float> depths;
    compat::Slice<float> baseDepths;
    compat::Slice<Vec2> projectedPoints;

    explicit DepthTargetView(Target target);
    virtual ~DepthTargetView();
    DepthTargetView(const DepthTargetView&) = delete;
    DepthTargetView& operator=(const DepthTargetView&) = delete;

    virtual Target getTarget() { return target; }
    virtual std::shared_ptr<nicxlive::core::nodes::Node> targetNode();
    virtual compat::Slice<Vec2> getVertices();
    virtual compat::Slice<std::uint16_t> getIndices() { return indices.duplicate(); }
    virtual Vec2 boundsMin() { return minPoint; }
    virtual Vec2 boundsMax() { return maxPoint; }
    virtual void refreshGeometry();
    virtual Vec2 localVertex(std::size_t index);
    virtual Vec2 snapLocalPoint(Vec2 point);
    virtual std::ptrdiff_t nearestLocalVertexIndex(Vec2 point);
    virtual float depthDisplayScale() {
        return sharedDepthDisplayScale > 0.0f
            ? sharedDepthDisplayScale : ngDepthDisplayScaleForBounds(minPoint, maxPoint);
    }
    virtual void setDepthDisplayScale(float value) {
        sharedDepthDisplayScale = value > 0.0f ? value : 0.0f;
    }
    virtual Vec2 depthViewToModel(Vec2 point, DepthCamera3D& depthCamera, float depth = 0.0f) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        return unprojectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }
    virtual Vec2 modelToDepthView(Vec2 point, float depth, DepthCamera3D& depthCamera) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        return projectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }
    virtual Vec2 projectLocalPoint(Vec2 point, float depth, DepthCamera3D& depthCamera) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        return modelToDepthView(point, depth, depthCamera);
    }
    virtual Vec2 localToWorld(Vec2 point) { return point; }
    virtual Vec2 worldToLocal(Vec2 point) { return point; }
    virtual Vec2 displayWorldToLocal(Vec2 point, DepthCamera3D& depthCamera, float depth = 0.0f) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        return depthViewToModel(point, depthCamera, depth);
    }
    virtual Vec2 projectedVertex(std::size_t index) {
        return index < projectedPoints.size() ? projectedPoints[index] : Vec2{0.0f, 0.0f};
    }
    virtual std::ptrdiff_t nearestProjectedVertex(Vec2 point, float radius);
    virtual compat::Slice<float> copyWorkingDepths() { return depths.duplicate(); }
    virtual void replaceWorkingDepths(compat::Slice<float> values);
    virtual void resetWorkingDepths();
    virtual void clearBaseDepths();
    virtual void resetFromTarget();
    virtual float getDepth(std::size_t index) { return index < depths.size() ? depths[index] : 0.0f; }
    virtual float depthAtLocalPoint(Vec2 point);
    virtual void setDepth(std::size_t index, float value) {
        if (index >= depths.size()) return;
        depths[index] = ngDepthTargetClampDepth(value);
    }
    virtual void addDepth(std::size_t index, float value) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        setDepth(index, getDepth(index) + value);
    }
    virtual void normalizeDepthLength();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthTargetView> other);
    virtual int opCmp(std::shared_ptr<DepthTargetView> other);
protected:
    virtual void prepareSourceClassInfo();
};

bool ngDepthTargetViewObjectEquals(std::shared_ptr<DepthTargetView> left,
    std::shared_ptr<DepthTargetView> right);

} // namespace nijigenerate::viewport::depth::common::targetview

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::common::targetview::DepthTargetView> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.common.targetview.DepthTargetView)";
    inline static constexpr char unqualifiedArgument[] = "(DepthTargetView)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.common.targetview.DepthTargetView";
    inline static constexpr char unqualifiedElement[] = "DepthTargetView";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::common::targetview::DepthTargetView> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.common.targetview.DepthTargetView";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.common.targetview.DepthTargetView)";
};
}
