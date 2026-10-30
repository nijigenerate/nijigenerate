#pragma once
#include "nijigenerate/backend/runtime_snapshot.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <math/camera.hpp>
#include <math/veca.hpp>
#include <render/common.hpp>
#include <array>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::model::onionslice {
struct OnionSliceGeometry {
    nicxlive::core::Rect area;
    nicxlive::core::math::Mat4 worldMatrix;
    OnionSliceGeometry();
    OnionSliceGeometry(nicxlive::core::Rect area, nicxlive::core::math::Mat4 camera,
        nicxlive::core::math::Mat4 render, nicxlive::core::math::Mat4 model);
    OnionSliceGeometry(nicxlive::core::math::Vec2Array vertices, nicxlive::core::math::Mat4 camera,
        nicxlive::core::math::Mat4 render, nicxlive::core::math::Mat4 model);
    nicxlive::core::math::Mat4 matrix(nicxlive::core::math::Mat4 camera) const;
};
namespace detail {
class OnionSliceCamera : public nicxlive::core::math::Camera {
public:
    OnionSliceCamera() = default;
    OnionSliceCamera(const OnionSliceCamera&) = delete;
    OnionSliceCamera& operator=(const OnionSliceCamera&) = delete;
    OnionSliceGeometry geometry;
    nicxlive::core::math::Mat4 matrix() const override;
    nicxlive::core::math::Mat4 matrix() override;
};
struct OnionSliceFrame {
    std::shared_ptr<nicxlive::core::Texture> texture;
    OnionSliceGeometry geometry;
};
}
class OnionSlice : public std::enable_shared_from_this<OnionSlice> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
public:
    virtual ~OnionSlice() = default;
    static std::shared_ptr<OnionSlice> singleton();
    virtual void start();
    virtual void stop();
    virtual void toggle();
    virtual void capture(std::array<std::uint32_t, 2> keypoint);
    virtual void draw();
    virtual bool enabled();
    virtual nicxlive::core::d_runtime::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<OnionSlice> other);
    virtual int opCmp(std::shared_ptr<OnionSlice> other);
private:
    OnionSlice();
    OnionSlice(const OnionSlice&) = delete;
    OnionSlice& operator=(const OnionSlice&) = delete;
    int numHistory;
    compat::Slice<detail::OnionSliceFrame> history;
    std::shared_ptr<detail::OnionSliceCamera> overlayCamera;
    std::shared_ptr<backend::snapshot::Snapshot> snapshot;
    int lastIndex;
    float opacity_;
    nicxlive::core::math::Vec3 color_;
    std::array<std::uint32_t, 2> lastKeypoint;
    bool hasLastKeypoint = false;
    std::shared_ptr<nicxlive::core::Puppet> activePuppet;
    static thread_local std::shared_ptr<OnionSlice> instance;
};
bool ngOnionSliceObjectEquals(std::shared_ptr<OnionSlice> left, std::shared_ptr<OnionSlice> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::onionslice::OnionSliceGeometry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.onionslice.OnionSliceGeometry)";
    inline static constexpr char unqualifiedArgument[] = "(OnionSliceGeometry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.onionslice.OnionSliceGeometry";
    inline static constexpr char unqualifiedElement[] = "OnionSliceGeometry";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::onionslice::detail::OnionSliceFrame> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.onionslice.OnionSliceFrame)";
    inline static constexpr char unqualifiedArgument[] = "(OnionSliceFrame)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.onionslice.OnionSliceFrame";
    inline static constexpr char unqualifiedElement[] = "OnionSliceFrame";
};

template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::onionslice::OnionSlice> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.onionslice.OnionSlice)";
    inline static constexpr char unqualifiedArgument[] = "(OnionSlice)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.onionslice.OnionSlice";
    inline static constexpr char unqualifiedElement[] = "OnionSlice";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::onionslice::OnionSlice> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.onionslice.OnionSlice";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.model.onionslice.OnionSlice)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::onionslice::detail::OnionSliceCamera> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.onionslice.OnionSliceCamera)";
    inline static constexpr char unqualifiedArgument[] = "(OnionSliceCamera)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.onionslice.OnionSliceCamera";
    inline static constexpr char unqualifiedElement[] = "OnionSliceCamera";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::onionslice::detail::OnionSliceCamera> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.onionslice.OnionSliceCamera";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.model.onionslice.OnionSliceCamera)";
};
}
