/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
*/
#pragma once

#include "package.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <common/resource_key_map.hpp>
#include <math/transform.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>

namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::ext::nodes { class ExCamera; }

namespace nijigenerate::actions::camera {

class CameraResizeAction : public Action, public std::enable_shared_from_this<CameraResizeAction> {
public:
    using Transform = nicxlive::core::math::Transform;
    using Vec2 = nicxlive::core::math::Vec2;
    using Children = compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>;
    struct ChildTransformKeyHash {
        std::size_t operator()(std::uint32_t key) const noexcept { return static_cast<std::size_t>(key); }
    };
    using ChildTransforms = nicxlive::core::common::ResourceKeyMap<std::uint32_t, Transform,
        ChildTransformKeyHash, std::equal_to<std::uint32_t>>;

    std::shared_ptr<ext::nodes::ExCamera> camera;
    Transform oldTransform;
    Transform newTransform;
    Vec2 oldViewport{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2 newViewport{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Children children;
    ChildTransforms oldChildTransforms;
    ChildTransforms newChildTransforms;

    CameraResizeAction(std::shared_ptr<ext::nodes::ExCamera> camera,
        Transform oldTransform, Vec2 oldViewport, Transform newTransform, Vec2 newViewport,
        Children children = {}, ChildTransforms oldChildTransforms = {}, ChildTransforms newChildTransforms = {});
    template<class Arguments> static std::shared_ptr<CameraResizeAction> createSource(Arguments&& arguments) {
        auto result=std::allocate_shared<CameraResizeAction>(
            nicxlive::core::d_runtime::SourceAllocator<CameraResizeAction>{},SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        },std::forward<Arguments>(arguments)());
        return result;
    }
    CameraResizeAction(const CameraResizeAction&) = delete;
    CameraResizeAction& operator=(const CameraResizeAction&) = delete;
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    virtual compat::RetainedString toString();

private:
    struct SourceAllocationTag {};
    explicit CameraResizeAction(SourceAllocationTag);
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(std::shared_ptr<ext::nodes::ExCamera> camera,
        Transform oldTransform,Vec2 oldViewport,Transform newTransform,Vec2 newViewport,
        Children children={},ChildTransforms oldChildTransforms={},ChildTransforms newChildTransforms={});
    void apply(Transform transform, Vec2 viewport, ChildTransforms childTransforms);
    void notify();
};

} // namespace nijigenerate::actions::camera

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::camera::CameraResizeAction> {
    inline static constexpr char className[]="nijigenerate.actions.camera.CameraResizeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.camera.CameraResizeAction)";
};

template<> struct SourceTypeSpellings<::nijigenerate::actions::camera::CameraResizeAction> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.actions.camera.CameraResizeAction)"),
            RetainedString::fromStaticLiteral("(CameraResizeAction)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.camera.CameraResizeAction"),
            RetainedString::fromStaticLiteral("CameraResizeAction")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::camera::CameraResizeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::camera::CameraResizeAction>> {};

} // namespace nijigenerate::core::translation
