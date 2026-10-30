#pragma once
#include <nodes/node.hpp>
#include <math/camera.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace nijigenerate::ext::nodes {
class ExCamera : public nicxlive::core::nodes::Node {
public:
    using Vec2 = nicxlive::core::math::Vec2;
    using Transform = nicxlive::core::math::Transform;
    using Camera = nicxlive::core::math::Camera;
    ExCamera();
    explicit ExCamera(Vec2 viewport);
    // The parent constructor attaches only after shared ownership exists.
    static std::shared_ptr<ExCamera> create(std::shared_ptr<Node> parent);
    template<class ParentProducer>
    static std::shared_ptr<ExCamera> createSourceDeferred(ParentProducer&& produceParent) {
        auto receiver = std::allocate_shared<ExCamera>(
            nicxlive::core::d_runtime::SourceAllocator<ExCamera>{}, DeferredParent{});
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        receiver->constructWithParent(parent);
        return receiver;
    }
    nicxlive::core::common::NullableString typeId() const override;
    void serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        nicxlive::core::nodes::SerializeNodeFlags flags = nicxlive::core::nodes::SerializeNodeFlags::All) const override;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) override;
    std::array<float, 4> getInitialBoundsSize() override;
    void drawBounds() override;
    virtual std::shared_ptr<Camera> getCamera();
    virtual Vec2& getViewport();
    virtual void setViewport(Vec2 value);
    virtual void foldScaleIntoViewport();
protected:
    Vec2 viewport{1920, 1080};
private:
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    struct DeferredParent {};
    explicit ExCamera(DeferredParent);
    struct ChildTransformKeyHash {
        std::size_t operator()(std::uint32_t key) const noexcept {
            return static_cast<std::size_t>(key);
        }
    };
    using ChildTransforms = nicxlive::core::common::ResourceKeyMap<std::uint32_t, Transform,
        ChildTransformKeyHash, std::equal_to<std::uint32_t>>;
    static Vec2 normalizeViewport(Vec2 value);
    static float evenDimension(float value);
    static float scaleSign(float value);
    ChildTransforms captureChildWorldTransforms();
    Transform cameraTransformWithScale(Vec2 scale);
    ChildTransforms captureChildWorldTransformsWithCameraScale(Vec2 scale);
    void restoreChildWorldTransforms(ChildTransforms childWorldTransforms);
    static float nonZeroScale(float value);
    static Transform rootTransformFor(const std::shared_ptr<Node>& child);
    static void restoreWorldTransformUnder(const std::shared_ptr<Node>& child,
        const std::shared_ptr<Node>& parent, Transform worldTransform);
};
void ngRegisterExCamera();
bool ngVerifyCameraSizeShowWarning(std::shared_ptr<ExCamera>& selectedCamera);
inline void incRegisterExCamera() { ngRegisterExCamera(); }
inline bool incVerifyCameraSizeShowWarning(std::shared_ptr<ExCamera>& selectedCamera) {
    return ngVerifyCameraSizeShowWarning(selectedCamera);
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExCamera> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.excamera.ExCamera";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.excamera.ExCamera)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExCamera> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.excamera.ExCamera)";
    inline static constexpr char unqualifiedArgument[] = "(ExCamera)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.excamera.ExCamera";
    inline static constexpr char unqualifiedElement[] = "ExCamera";
};
}

namespace nijigenerate::ext::nodes::excamera {
using ::nijigenerate::ext::nodes::ExCamera;
using ::nijigenerate::ext::nodes::ngRegisterExCamera;
using ::nijigenerate::ext::nodes::ngVerifyCameraSizeShowWarning;
using ::nijigenerate::ext::nodes::incRegisterExCamera;
using ::nijigenerate::ext::nodes::incVerifyCameraSizeShowWarning;
}
