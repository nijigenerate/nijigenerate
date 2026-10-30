#pragma once

#include <math/types.hpp>
#include <math/veca.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/const_slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/viewport/depth/camera.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::depth::mesheditor::node { class DepthMeshEditorOne; }
namespace nijigenerate::ext::nodes { struct ExDepthOp; }
namespace nicxlive::core::nodes { class GridDeformer; }

namespace nijigenerate::viewport::depth::tools::operation {

namespace detail {
template<class Array> decltype(auto) sourceElement(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/tools/operation.d"), line);
    return values.data()[index];
}
}

inline constexpr nicxlive::core::math::Vec4 DepthOperationColor{0.1f,0.4f,0.9f,0.9f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationSelectedColor{0.1f,0.4f,0.9f,1.0f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationPositiveColor{0.05f,0.62f,1.0f,0.92f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationPositiveSelectedColor{0.0f,0.8f,1.0f,1.0f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationNegativeColor{1.0f,0.32f,0.18f,0.92f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationNegativeSelectedColor{1.0f,0.48f,0.22f,1.0f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationHandleColor{0.1f,0.65f,1.0f,1.0f};
inline constexpr nicxlive::core::math::Vec4 DepthOperationAmountColor{1.0f,0.9f,0.2f,1.0f};

enum class DepthOperationHandle : std::int32_t { None, Body, P0, P1, Amount, RadiusX, RadiusY, P0Angle, P1Angle };
struct DepthOperationHandleSwitchError { DepthOperationHandle value; };
enum class RingOrientation : std::int32_t { Horizontal, Vertical };

using DepthEditorHandle=std::shared_ptr<mesheditor::node::DepthMeshEditorOne>;
void drawDepthPoint(nicxlive::core::math::Vec2 point, nicxlive::core::math::Vec4 color, float size);
void drawDepthPoint(DepthEditorHandle editor, nicxlive::core::math::Vec2 local, float depth,
    DepthCamera3D& camera, nicxlive::core::math::Vec4 color, float size);
void drawDepthLine(DepthEditorHandle editor, nicxlive::core::math::Vec2 p0, nicxlive::core::math::Vec2 p1,
    DepthCamera3D& camera, nicxlive::core::math::Vec4 color, float width = 2.5f);
void drawDepthLinePoints(nicxlive::core::math::Vec2 p0, nicxlive::core::math::Vec2 p1,
    nicxlive::core::math::Vec4 color, float width = 2.5f);
void drawDepthEllipse(DepthEditorHandle editor, nicxlive::core::math::Vec2 center, float radiusX,
    float radiusY, float angleDeg, float depth, DepthCamera3D& camera,
    nicxlive::core::math::Vec4 color, float width = 2.0f);
void drawDepthPolyline(nicxlive::core::math::Vec3Array points, nicxlive::core::math::Vec4 color, float width);
compat::Slice<float> ngComputeDepthsFromOps(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid,
    compat::Slice<ext::nodes::ExDepthOp> operations, compat::ConstSlice<float> baseDepths);

class DepthOperation : public std::enable_shared_from_this<DepthOperation> {
    using std::enable_shared_from_this<DepthOperation>::shared_from_this;
    using std::enable_shared_from_this<DepthOperation>::weak_from_this;
protected:
    std::shared_ptr<DepthOperation> retainedSelf() {return shared_from_this();}
    std::shared_ptr<DepthOperation> retainReceiver() noexcept {return weak_from_this().lock();}
public:
    DepthOperation()=default;
    virtual ~DepthOperation()=default;
    DepthOperation(const DepthOperation&)=delete;
    DepthOperation& operator=(const DepthOperation&)=delete;
    virtual std::shared_ptr<DepthOperation> clone()=0;
    virtual void apply(DepthEditorHandle editor)=0;
    virtual void draw(DepthEditorHandle editor,DepthCamera3D& camera,bool selected,DepthOperationHandle hotHandle)=0;
    virtual DepthOperationHandle hit(DepthEditorHandle editor,nicxlive::core::math::Vec2 mouse,
        DepthCamera3D& camera,float radius,float& distance)=0;
    virtual void drag(DepthOperationHandle handle,DepthEditorHandle editor,std::shared_ptr<DepthOperation> startOperation,
        nicxlive::core::math::Vec2 startLocal,nicxlive::core::math::Vec2 currentLocal,
        float startMouseY,float currentMouseY,bool snapToGrid)=0;
    virtual std::string label()=0;
    virtual std::string valueLabel()=0;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthOperation> other);
    virtual int opCmp(std::shared_ptr<DepthOperation> other);
};

class DepthAttachedPointOperation : public DepthOperation {
    struct SourceAllocationTag {};
    explicit DepthAttachedPointOperation(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::size_t index, float amount);
public:
    std::size_t index=0;
    float amount=std::numeric_limits<float>::quiet_NaN();

    DepthAttachedPointOperation(std::size_t index,float amount);
    template<class Arguments> static std::shared_ptr<DepthAttachedPointOperation> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthAttachedPointOperation>(
            nicxlive::core::d_runtime::SourceAllocator<DepthAttachedPointOperation>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    std::shared_ptr<DepthOperation> clone() override;
    void apply(DepthEditorHandle editor) override;
    void draw(DepthEditorHandle editor,DepthCamera3D& camera,bool selected,DepthOperationHandle hotHandle) override;
    DepthOperationHandle hit(DepthEditorHandle editor,nicxlive::core::math::Vec2 mouse,
        DepthCamera3D& camera,float radius,float& distance) override;
    void drag(DepthOperationHandle handle,DepthEditorHandle editor,std::shared_ptr<DepthOperation> startOperation,
        nicxlive::core::math::Vec2 startLocal,nicxlive::core::math::Vec2 currentLocal,
        float startMouseY,float currentMouseY,bool snapToGrid) override;
    std::string label() override;
    std::string valueLabel() override;
};

class DepthRingOperation : public DepthOperation {
    struct SourceAllocationTag {};
    explicit DepthRingOperation(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(nicxlive::core::math::Vec2 p0,
        nicxlive::core::math::Vec2 p1, DepthBrushSettings settings);
public:
    nicxlive::core::math::Vec2 p0{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec2 p1{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    float amount=std::numeric_limits<float>::quiet_NaN();
    float width=std::numeric_limits<float>::quiet_NaN();
    float hardness=std::numeric_limits<float>::quiet_NaN();
    float p0Angle=180.0f;
    float p1Angle=0.0f;

    DepthRingOperation(nicxlive::core::math::Vec2 p0,nicxlive::core::math::Vec2 p1,DepthBrushSettings settings);
    template<class Arguments> static std::shared_ptr<DepthRingOperation> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthRingOperation>(
            nicxlive::core::d_runtime::SourceAllocator<DepthRingOperation>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    std::shared_ptr<DepthOperation> clone() override;
    void apply(DepthEditorHandle editor) override;
    void draw(DepthEditorHandle editor,DepthCamera3D& camera,bool selected,DepthOperationHandle hotHandle) override;
    DepthOperationHandle hit(DepthEditorHandle editor,nicxlive::core::math::Vec2 mouse,
        DepthCamera3D& camera,float radius,float& distance) override;
    void drag(DepthOperationHandle handle,DepthEditorHandle editor,std::shared_ptr<DepthOperation> startOperation,
        nicxlive::core::math::Vec2 startLocal,nicxlive::core::math::Vec2 currentLocal,
        float startMouseY,float currentMouseY,bool snapToGrid) override;
    std::string label() override;
    std::string valueLabel() override;
};

class DepthPlaneOperation : public DepthOperation {
    struct SourceAllocationTag {};
    explicit DepthPlaneOperation(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(nicxlive::core::math::Vec2 center, float radiusX,
        float radiusY, DepthBrushSettings settings);
public:
    nicxlive::core::math::Vec2 center{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    float radiusX=std::numeric_limits<float>::quiet_NaN();
    float radiusY=std::numeric_limits<float>::quiet_NaN();
    float angle=std::numeric_limits<float>::quiet_NaN();
    float targetDepth=std::numeric_limits<float>::quiet_NaN();
    float flattenStrength=std::numeric_limits<float>::quiet_NaN();

    DepthPlaneOperation(nicxlive::core::math::Vec2 center,float radiusX,float radiusY,DepthBrushSettings settings);
    template<class Arguments> static std::shared_ptr<DepthPlaneOperation> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthPlaneOperation>(
            nicxlive::core::d_runtime::SourceAllocator<DepthPlaneOperation>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    std::shared_ptr<DepthOperation> clone() override;
    void apply(DepthEditorHandle editor) override;
    void draw(DepthEditorHandle editor,DepthCamera3D& camera,bool selected,DepthOperationHandle hotHandle) override;
    DepthOperationHandle hit(DepthEditorHandle editor,nicxlive::core::math::Vec2 mouse,
        DepthCamera3D& camera,float radius,float& distance) override;
    void drag(DepthOperationHandle handle,DepthEditorHandle editor,std::shared_ptr<DepthOperation> startOperation,
        nicxlive::core::math::Vec2 startLocal,nicxlive::core::math::Vec2 currentLocal,
        float startMouseY,float currentMouseY,bool snapToGrid) override;
    std::string label() override;
    std::string valueLabel() override;
};

ext::nodes::ExDepthOp toExDepthOp(std::shared_ptr<DepthOperation> operation);
std::shared_ptr<DepthOperation> depthOperationFromExDepthOp(ext::nodes::ExDepthOp op);
RingOrientation ringOrientation(std::shared_ptr<DepthRingOperation> op);
float ringCrossCoord(std::shared_ptr<DepthRingOperation> op,std::uint8_t axis);

struct RingSample {
    float cross=std::numeric_limits<float>::quiet_NaN();
    float z=std::numeric_limits<float>::quiet_NaN();
    float normalSlope=std::numeric_limits<float>::quiet_NaN();
    float width=std::numeric_limits<float>::quiet_NaN();
    float hardness=std::numeric_limits<float>::quiet_NaN();
    std::size_t count=0;
};

void applyRingDepth(DepthEditorHandle editor,nicxlive::core::math::Vec2 p0,
    nicxlive::core::math::Vec2 p1,float amount,float width,float hardness);
void drawDepthRingCurve(DepthEditorHandle editor, std::shared_ptr<DepthRingOperation> operation,
    DepthCamera3D& camera, bool backSide, nicxlive::core::math::Vec4 color, float width);
bool ringSampleAt(std::shared_ptr<DepthRingOperation> op,nicxlive::core::math::Vec2 point,
    std::uint8_t axis,RingSample& sample);
void applyPlaneFlatten(DepthEditorHandle editor,nicxlive::core::math::Vec2 center,
    float radiusX,float radiusY,float angle,float targetDepth,float flattenStrength);
float ringBaseDepthAt(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,float ratio);
nicxlive::core::math::Vec2 ringEndpointPoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    bool first,DepthCamera3D& camera);
nicxlive::core::math::Vec2 ringCurvePoint(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    float ratio,bool backSide,DepthCamera3D& camera);
nicxlive::core::math::Vec2 ringAmountHandlePoint(DepthEditorHandle editor,
    std::shared_ptr<DepthRingOperation> op,DepthCamera3D& camera);
nicxlive::core::math::Vec2 ringAngleHandlePoint(DepthEditorHandle editor,
    std::shared_ptr<DepthRingOperation> op,bool first,DepthCamera3D& camera);
float ringCurveDistance(DepthEditorHandle editor,std::shared_ptr<DepthRingOperation> op,
    nicxlive::core::math::Vec2 mouse,DepthCamera3D& camera);
void applyRingNormalSurfaces(DepthEditorHandle editor,compat::Slice<std::shared_ptr<DepthRingOperation>> rings);
void applyRingFamilySurface(DepthEditorHandle editor,compat::Slice<std::shared_ptr<DepthRingOperation>> rings,
    std::uint8_t axis);

inline compat::Slice<RingSample> combineRingSamplesByCross(compat::Slice<RingSample> samples) {
    compat::Slice<RingSample> groups;
    constexpr float epsilon=0.0001f;
    for (const auto sample : samples) {
        std::ptrdiff_t groupIndex=-1;
        for (std::size_t i=0;i<groups.size();++i) {
            if (std::abs(groups[i].cross-sample.cross)<=epsilon) {
                groupIndex=static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
        if (groupIndex<0) { groups.append(sample);continue; }
        auto group=detail::sourceElement(groups,static_cast<std::size_t>(groupIndex),516);
        const auto count=group.count;
        group.z+=sample.z;
        group.normalSlope+=sample.normalSlope;
        group.width=std::max(group.width,sample.width);
        group.hardness=(group.hardness*static_cast<float>(count)+sample.hardness)/static_cast<float>(count+1);
        group.count=count+1;
        detail::sourceElement(groups,static_cast<std::size_t>(groupIndex),523)=group;
    }
    return groups;
}

inline float depthToolRound(float value) {
    return std::round(value * 1000.0f) / 1000.0f;
}

inline nicxlive::core::math::Vec4 depthOperationColor(float depth,bool selected=false) {
    if (depth>0.000001f) return selected?DepthOperationPositiveSelectedColor:DepthOperationPositiveColor;
    if (depth<-0.000001f) return selected?DepthOperationNegativeSelectedColor:DepthOperationNegativeColor;
    return selected?DepthOperationSelectedColor:DepthOperationColor;
}

namespace detail {
inline float segmentLength(float x,float y) {
    // Actual Windows D/inmath length accumulates float squares into real (double).
    double squared=0.0;
    squared+=static_cast<double>(x*x);
    squared+=static_cast<double>(y*y);
    return static_cast<float>(std::sqrt(squared));
}
}

inline float distanceToSegment(nicxlive::core::math::Vec2 point,nicxlive::core::math::Vec2 a,
    nicxlive::core::math::Vec2 b) {
    const float vx=b.x-a.x,vy=b.y-a.y;
    const float len2=vx*vx+vy*vy;
    if (len2<=0.000001f) return detail::segmentLength(point.x-a.x,point.y-a.y);
    const float quotient=((point.x-a.x)*vx+(point.y-a.y)*vy)/len2;
    const float t=quotient<0.0f?0.0f:quotient>1.0f?1.0f:quotient;
    const nicxlive::core::math::Vec2 closest{a.x+vx*t,a.y+vy*t};
    return detail::segmentLength(point.x-closest.x,point.y-closest.y);
}

inline float hermiteDepth(float z0,float m0,float z1,float m1,float span,float t) {
    const float t2=t*t,t3=t2*t;
    return (2*t3-3*t2+1)*z0+(t3-2*t2+t)*m0*span+(-2*t3+3*t2)*z1+(t3-t2)*m1*span;
}

inline float ellipseNorm(nicxlive::core::math::Vec2 point,nicxlive::core::math::Vec2 center,
    float radiusX,float radiusY,float angleDeg) {
    const float angle=-angleDeg*3.14159265358979323846f/180.0f;
    const float dx=point.x-center.x,dy=point.y-center.y;
    const float x=std::cos(angle)*dx-std::sin(angle)*dy;
    const float y=std::sin(angle)*dx+std::cos(angle)*dy;
    const float nx=x/(1.0f<radiusX?radiusX:1.0f),ny=y/(1.0f<radiusY?radiusY:1.0f);
    return std::sqrt(nx*nx+ny*ny);
}

} // namespace nijigenerate::viewport::depth::tools::operation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::DepthOperationHandle> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthOperationHandle)";
    inline static constexpr char unqualifiedArgument[] = "(DepthOperationHandle)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.tools.operation.DepthOperationHandle";
    inline static constexpr char unqualifiedElement[] = "DepthOperationHandle";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::RingOrientation> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.operation.RingOrientation)";
    inline static constexpr char unqualifiedArgument[] = "(RingOrientation)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.operation.RingOrientation";
    inline static constexpr char unqualifiedElement[] = "RingOrientation";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::RingSample> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.tools.operation.RingSample)";
    inline static constexpr char unqualifiedArgument[] = "(RingSample)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.operation.RingSample";
    inline static constexpr char unqualifiedElement[] = "RingSample";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::DepthOperation> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.tools.operation.DepthOperation)";
    inline static constexpr char unqualifiedArgument[] = "(DepthOperation)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.operation.DepthOperation";
    inline static constexpr char unqualifiedElement[] = "DepthOperation";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::tools::operation::DepthAttachedPointOperation> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthAttachedPointOperation)";
    inline static constexpr char unqualifiedArgument[] = "(DepthAttachedPointOperation)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.tools.operation.DepthAttachedPointOperation";
    inline static constexpr char unqualifiedElement[] = "DepthAttachedPointOperation";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::DepthRingOperation> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthRingOperation)";
    inline static constexpr char unqualifiedArgument[] = "(DepthRingOperation)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.operation.DepthRingOperation";
    inline static constexpr char unqualifiedElement[] = "DepthRingOperation";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::operation::DepthPlaneOperation> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthPlaneOperation)";
    inline static constexpr char unqualifiedArgument[] = "(DepthPlaneOperation)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.tools.operation.DepthPlaneOperation";
    inline static constexpr char unqualifiedElement[] = "DepthPlaneOperation";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::operation::DepthOperation> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.operation.DepthOperation";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthOperation)";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::depth::tools::operation::DepthAttachedPointOperation> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.operation.DepthAttachedPointOperation";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthAttachedPointOperation)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::operation::DepthRingOperation> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.operation.DepthRingOperation";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthRingOperation)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::operation::DepthPlaneOperation> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.operation.DepthPlaneOperation";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.operation.DepthPlaneOperation)";
};
} // namespace nicxlive::core::d_runtime
