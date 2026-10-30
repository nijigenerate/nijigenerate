#include "excamera.hpp"
#include "excamera_math.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <puppet.hpp>
#include <serde_reader.hpp>
#include <cmath>

namespace nijigenerate::ext::nodes {
using namespace nicxlive::core::math;
using namespace nicxlive::core::serde;
using nicxlive::core::nodes::SerializeNodeFlags;
using nicxlive::core::nodes::NotifyReason;
using nicxlive::core::nodes::Node;

namespace {
void registerCameraClassName() {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExCamera),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.excamera.ExCamera"));
}
}

ExCamera::ExCamera() {
    registerCameraClassName();
    constructWithParent();
}
ExCamera::ExCamera(DeferredParent) { registerCameraClassName(); }
ExCamera::ExCamera(Vec2 value) : ExCamera() { viewport = normalizeViewport(value); }
std::shared_ptr<ExCamera> ExCamera::create(std::shared_ptr<Node> parent) {
    auto result = std::allocate_shared<ExCamera>(
        nicxlive::core::d_runtime::SourceAllocator<ExCamera>{}, DeferredParent{});
    result->constructWithParent(parent);
    return result;
}
nicxlive::core::common::NullableString ExCamera::typeId() const {
    return nicxlive::core::common::NullableString::fromStaticLiteral("Camera");
}
void ExCamera::serializeSelfImpl(TypedJsonWriter& serializer, bool recursive, SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (nicxlive::core::nodes::has_flag(flags, SerializeNodeFlags::State))
        serializer.putField("viewport", std::array<float, 2>{viewport.x, viewport.y});
}
ReadError ExCamera::deserializeFromFghj(TypedJsonView data) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (auto error = Node::deserializeFromFghj(data)) return error;
    const auto entry = data.findFirst("viewport");
    if (!entry.isMissing()) {
        std::array<float, 2> values{viewport.x, viewport.y};
        try { (void)deserializeValue(entry, values); }
        catch (...) { viewport = Vec2{values[0], values[1]}; throw; }
        viewport = Vec2{values[0], values[1]};
    }
    viewport = normalizeViewport(viewport);
    return {};
}
std::array<float, 4> ExCamera::getInitialBoundsSize() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto tr = transform();
    const Vec2 viewportHalf{viewport.x / 2, viewport.y / 2};
    const auto scale = transform().scale;
    const Vec2 half{viewportHalf.x * scale.x, viewportHalf.y * scale.y};
    return {tr.translation.x - half.x, tr.translation.y - half.y,
        tr.translation.x + half.x, tr.translation.y + half.y};
}
void ExCamera::drawBounds() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    [[maybe_unused]] const auto tr = transform();
    const Vec2 half{viewport.x / 2, viewport.y / 2};
    const Vec3 topLeft{-half.x, -half.y, 0}, bottomRight{half.x, half.y, 0};
    const Vec3 topRight{bottomRight.x, topLeft.y, 0}, bottomLeft{topLeft.x, bottomRight.y, 0};
    const std::array<Vec3, 8> points{topLeft, topRight, topRight, bottomRight,
        bottomRight, bottomLeft, bottomLeft, topLeft};
    core::dbg::inDbgSetBuffer(std::span<const Vec3>(points));
    core::dbg::inDbgLineWidth(2);
    core::dbg::inDbgDrawLines(Vec4{0, 0, 0, 1}, transform().matrix());
    core::dbg::inDbgLineWidth(1);
    core::dbg::inDbgDrawLines(Vec4{1, 1, 1, 1}, transform().matrix());
}
std::shared_ptr<ExCamera::Camera> ExCamera::getCamera() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto scale = transform().scale;
    auto camera = std::allocate_shared<Camera>(nicxlive::core::d_runtime::SourceAllocator<Camera>{});
    const auto position = transform().translation;
    camera->position = Vec2{position.x * -1, position.y * -1};
    camera->rotation = -transform().rotation.z;
    camera->scale = Vec2{1 / scale.x, 1 / scale.y};
    return camera;
}
ExCamera::Vec2& ExCamera::getViewport() { return viewport; }
void ExCamera::setViewport(Vec2 value) { viewport = normalizeViewport(value); }
void ExCamera::foldScaleIntoViewport() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto scale = localTransform.scale;
    if (scale.x == 1 && scale.y == 1) return;
    const auto captured = captureChildWorldTransformsWithCameraScale(Vec2{1, 1});
    setViewport(Vec2{viewport.x * std::abs(scale.x), viewport.y * std::abs(scale.y)});
    localTransform.scale = Vec2{scaleSign(scale.x), scaleSign(scale.y)};
    localTransform.update();
    transformChanged();
    restoreChildWorldTransforms(captured);
}
ExCamera::Vec2 ExCamera::normalizeViewport(Vec2 value) {
    return Vec2{evenDimension(value.x), evenDimension(value.y)};
}
float ExCamera::evenDimension(float value) {
    const float size = std::abs(value);
    if (!std::isfinite(size)) return 2;
    const float roundedValue = std::round(size);
    // Out-of-range D/x86 signed conversion yields INT_MIN and then the minimum.
    if (roundedValue >= 2147483648.0f) return 2;
    auto rounded = static_cast<std::int32_t>(roundedValue);
    if (rounded < 2) return 2;
    if (rounded % 2 != 0) ++rounded;
    return static_cast<float>(rounded);
}
float ExCamera::scaleSign(float value) { return value < 0 ? -1.0f : 1.0f; }
ExCamera::ChildTransforms ExCamera::captureChildWorldTransforms() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    ChildTransforms result;
    const auto capturedChildren = children;
    for (auto child : capturedChildren) {
        auto& destination = result;
        const auto key = compat::ngRequireObject(child)->getUUID();
        const auto value = compat::ngRequireObject(child)->transform();
        destination[key] = value;
    }
    return result;
}
ExCamera::Transform ExCamera::cameraTransformWithScale(Vec2 scale) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto cameraLocal = localTransform;
    cameraLocal.scale = scale;
    cameraLocal.update();
    if (const auto parent = parentRef()) return cameraLocal * parent->transform();
    return cameraLocal;
}
ExCamera::ChildTransforms ExCamera::captureChildWorldTransformsWithCameraScale(Vec2 scale) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto cameraTransform = cameraTransformWithScale(scale);
    ChildTransforms result;
    const auto capturedChildren = children;
    for (auto child : capturedChildren) {
        if (compat::ngRequireObject(child)->lockToRootValue()) {
            auto& destination = result;
            const auto key = compat::ngRequireObject(child)->getUUID();
            const auto value = compat::ngRequireObject(child)->transform();
            destination[key] = value;
            continue;
        }
        compat::ngRequireObject(child)->localTransform.update();
        auto& destination = result;
        const auto key = compat::ngRequireObject(child)->getUUID();
        const auto value = compat::ngRequireObject(child)->localTransform * cameraTransform;
        destination[key] = value;
    }
    return result;
}
void ExCamera::restoreChildWorldTransforms(ChildTransforms values) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto capturedChildren = children;
    for (auto child : capturedChildren) {
        if (const auto entry = values.find(compat::ngRequireObject(child)->getUUID()); entry != values.end()) {
            restoreWorldTransformUnder(child, shared_from_this(), entry->second);
            compat::ngRequireObject(child)->notifyChange(child, NotifyReason::AttributeChanged);
            compat::ngRequireObject(child)->notifyChange(child, NotifyReason::Transformed);
        }
    }
}
float ExCamera::nonZeroScale(float value) { return std::abs(value) < 0.0001f ? 1.0f : value; }
ExCamera::Transform ExCamera::rootTransformFor(const std::shared_ptr<Node>& child) {
    const auto puppet = compat::ngRequireObject(child)->puppetRef();
    if (puppet && puppet->root) return puppet->root->localTransform;
    return Transform(Vec3{0, 0, 0});
}
void ExCamera::restoreWorldTransformUnder(const std::shared_ptr<Node>& child,
        const std::shared_ptr<Node>& parent, Transform world) {
    const Transform parentTransform = compat::ngRequireObject(child)->lockToRootValue() ? rootTransformFor(child)
        : (parent ? parent->transform() : Transform(Vec3{0, 0, 0}));
    const Vec3 rotation{world.rotation.x - parentTransform.rotation.x,
        world.rotation.y - parentTransform.rotation.y, world.rotation.z - parentTransform.rotation.z};
    const Vec2 scale{world.scale.x / nonZeroScale(parentTransform.scale.x),
        world.scale.y / nonZeroScale(parentTransform.scale.y)};
    const auto basis = excamera_detail::basis(rotation, scale);
    const auto point = excamera_detail::point(excamera_detail::inverse(parentTransform.matrix()),
        Vec4{world.translation.x, world.translation.y, world.translation.z, 1});
    child->localTransform.translation = Vec3{
        point.x - basis.x - child->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x")),
        point.y - basis.y - child->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y")),
        point.z - basis.z - child->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.z"))};
    child->localTransform.rotation = Vec3{
        rotation.x - child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.x")),
        rotation.y - child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.y")),
        rotation.z - child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.z"))};
    child->localTransform.scale = Vec2{
        scale.x / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.x"))),
        scale.y / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.y")))};
    child->localTransform.update();
    child->transformChanged();
}
void ngRegisterExCamera() {
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("Camera"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return ExCamera::create(parent);
        }));
}
bool ngVerifyCameraSizeShowWarning(std::shared_ptr<ExCamera>& camera) {
    if (std::fmod(compat::ngRequireObject(camera)->getViewport().x, 2.0f) != 0 ||
        std::fmod(compat::ngRequireObject(camera)->getViewport().y, 2.0f) != 0) {
        widgets::ngTextColored(ImVec4(1, 0, 0, 1),
            compat::RetainedStringSlice(core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Warning: Camera size must be divisible by 2"))));
        return true;
    }
    return false;
}
}
