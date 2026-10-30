#include "transformhandle.hpp"
#include "drawable_overlay.hpp"
#include <nijigenerate/actions/camera.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <puppet.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <source_location>
#include <tuple>
#include <typeinfo>
#include <utility>

namespace nijigenerate::viewport::common::transformhandle {
namespace {
using namespace nicxlive::core::math;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::NotifyReason;
using ext::nodes::ExCamera;
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using actions::camera::CameraResizeAction;
using AdapterPtr = std::shared_ptr<ViewportTransformHandleAdapter>;
using TransformMap = CameraResizeAction::ChildTransforms;

Vec2 point(const Mat4& matrix, Vec2 value) {
    const auto result = ext::nodes::excamera_detail::point(matrix, Vec4{value.x, value.y, 0, 1});
    return Vec2{result.x, result.y};
}
void unionBounds(Vec4& target, Vec4 candidate) {
    target = Vec4{std::min(target.x, candidate.x), std::min(target.y, candidate.y),
        std::max(target.z, candidate.z), std::max(target.w, candidate.w)};
}
bool selfMeshBounds(NodePtr node, Vec4& result) {
    const float initial = std::numeric_limits<float>::quiet_NaN();
    result = Vec4{initial, initial, initial, initial};
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
        if (drawable->getVertices().empty()) return false;
        const auto matrix = ngMeshOverlayMatrix(*drawable);
        auto points = ngMeshOverlayPoints(*drawable);
        if (points.empty()) return false;
        const auto first = point(matrix, points[0].toVector());
        result = Vec4{first.x, first.y, first.x, first.y};
        for (std::size_t i = 1; i < points.size(); ++i) {
            const auto value = point(matrix, points[i].toVector());
            unionBounds(result, Vec4{value.x, value.y, value.x, value.y});
        }
        return true;
    }
    if (auto deformable = std::dynamic_pointer_cast<Deformable>(node)) {
        auto vertices = deformable->getVertices();
        if (vertices.empty()) return false;
        const auto matrix = deformable->getDynamicMatrix();
        const auto deformCount = deformable->deformation.size();
        auto first = vertices[0].toVector();
        if (deformCount > 0) {
            const auto deformation = deformable->deformation[0].toVector();
            first.x += deformation.x; first.y += deformation.y;
        }
        first = point(matrix, first);
        result = Vec4{first.x, first.y, first.x, first.y};
        for (std::size_t i = 1; i < vertices.size(); ++i) {
            auto value = vertices[i].toVector();
            if (i < deformCount) {
                const auto deformation = deformable->deformation[i].toVector();
                value.x += deformation.x; value.y += deformation.y;
            }
            value = point(matrix, value);
            unionBounds(result, Vec4{value.x, value.y, value.x, value.y});
        }
        return true;
    }
    return false;
}
float scaleSign(float value) { return value < 0 ? -1.0f : 1.0f; }
float nonZeroScale(float value) { return std::abs(value) < 0.0001f ? 1.0f : value; }
Transform rootTransformFor(NodePtr child) {
    actions::node::detail::requireActionObject(child);
    const auto puppet = child->puppetRef();
    if (puppet && puppet->root) return puppet->root->localTransform;
    return Transform(Vec3{0, 0, 0});
}
void restoreWorldTransformUnder(NodePtr child, NodePtr parent, Transform world) {
    actions::node::detail::requireActionObject(child);
    const auto parentTransform = child->lockToRootValue() ? rootTransformFor(child)
        : (parent ? parent->transform() : Transform(Vec3{0, 0, 0}));
    const auto translation = Node::getRelativePosition(parentTransform.matrix(), world.matrix());
    const Vec3 rotation{world.rotation.x - parentTransform.rotation.x,
        world.rotation.y - parentTransform.rotation.y, world.rotation.z - parentTransform.rotation.z};
    const Vec2 scale{world.scale.x / nonZeroScale(parentTransform.scale.x),
        world.scale.y / nonZeroScale(parentTransform.scale.y)};
    {
        auto& destination=child->localTransform.translation;
        const Vec3 offset{
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.x")),
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.y")),
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.z"))};
        destination=Vec3{translation.x-offset.x,translation.y-offset.y,translation.z-offset.z};
    }
    {
        auto& destination=child->localTransform.rotation;
        const Vec3 offset{
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.x")),
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.y")),
            child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.z"))};
        destination=Vec3{rotation.x-offset.x,rotation.y-offset.y,rotation.z-offset.z};
    }
    {
        auto& destination=child->localTransform.scale;
        destination=Vec2{
            scale.x / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.x"))),
            scale.y / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.y")))};
    }
    child->localTransform.update();
    child->transformChanged();
}
class DefaultViewportTransformHandleAdapter : public ViewportTransformHandleAdapter {
public:
    Vec4 bounds(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        actions::node::detail::requireActionObject(node);
        const auto combined = node->getCombinedBounds();
        Vec4 result{combined[0], combined[1], combined[2], combined[3]};
        Vec4 ownBounds;
        if (selfMeshBounds(node, ownBounds)) unionBounds(result, ownBounds);
        return result;
    }
    Vec2 scaleHandleValue(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        return actions::node::detail::requireActionObject(node)->localTransform.scale;
    }
    void beginScale(NodePtr) override {}
    void applyScale(NodePtr node, Vec2 scale) override {
        const auto receiver=weak_from_this().lock();
        actions::node::detail::requireActionObject(node);
        node->localTransform.scale = scale;
        node->notifyChange(node, NotifyReason::AttributeChanged);
    }
    void endScale(NodePtr node, Vec2 previous, ActionMap& result) override {
        const auto receiver=weak_from_this().lock();
        using Change = actions::node::NodeValueChangeAction<Node, float>;
        actions::node::detail::requireActionObject(node);
        if (node->localTransform.scale.x != previous.x) {
            auto& output=result;
            const auto key=compat::RetainedString::fromStaticLiteral("X");
            const auto action=std::allocate_shared<Change>(
                nicxlive::core::d_runtime::SourceAllocator<Change>{},key, node,
                previous.x, node->localTransform.scale.x, std::shared_ptr<float>(node, &node->localTransform.scale.x));
            output[key]=action;
        }
        if (node->localTransform.scale.y != previous.y) {
            auto& output=result;
            const auto key=compat::RetainedString::fromStaticLiteral("Y");
            const auto action=std::allocate_shared<Change>(
                nicxlive::core::d_runtime::SourceAllocator<Change>{},key, node,
                previous.y, node->localTransform.scale.y, std::shared_ptr<float>(node, &node->localTransform.scale.y));
            output[key]=action;
        }
    }
};
class NodeOriginViewportTransformHandleAdapter : public DefaultViewportTransformHandleAdapter {
public:
    Vec4 bounds(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        actions::node::detail::requireActionObject(node);
        const auto origin = point(node->transform().matrix(), Vec2{0, 0});
        return Vec4{origin.x, origin.y, origin.x, origin.y};
    }
};
class CameraViewportTransformHandleAdapter : public ViewportTransformHandleAdapter {
    std::shared_ptr<ExCamera> activeCamera;
    Transform oldTransform;
    Vec2 oldViewport{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    CameraResizeAction::Children children;
    TransformMap oldChildTransforms;
    TransformMap childWorldTransforms;
    std::shared_ptr<ExCamera> camera(NodePtr node) {
        auto result = std::dynamic_pointer_cast<ExCamera>(node);
#ifndef NDEBUG
        if (!result) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/transformhandle.d"), 80);
#endif
        return result;
    }
    void ensureActive(std::shared_ptr<ExCamera> value) {
        if (activeCamera != value) beginScale(value);
    }
public:
    Vec4 bounds(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        const auto value = camera(node);
        actions::node::detail::requireActionObject(value);
        const auto viewport = value->getViewport();
        const Vec2 half{viewport.x / 2, viewport.y / 2};
        const Vec2 corners[]{Vec2{-half.x, -half.y}, Vec2{half.x, -half.y},
            Vec2{-half.x, half.y}, Vec2{half.x, half.y}};
        const auto matrix = value->transform().matrix();
        const auto first = point(matrix, corners[0]);
        Vec4 result{first.x, first.y, first.x, first.y};
        for (std::size_t i = 1; i < 4; ++i) {
            const auto item = point(matrix, corners[i]);
            unionBounds(result, Vec4{item.x, item.y, item.x, item.y});
        }
        return result;
    }
    Vec2 scaleHandleValue(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        return actions::node::detail::requireActionObject(node)->localTransform.scale;
    }
    void beginScale(NodePtr node) override {
        const auto receiver=weak_from_this().lock();
        activeCamera = camera(node);
        actions::node::detail::requireActionObject(activeCamera);
        oldTransform = activeCamera->localTransform;
        oldViewport = actions::node::detail::requireActionObject(activeCamera)->getViewport();
        const auto currentChildren = actions::node::detail::requireActionObject(activeCamera)->children;
        CameraResizeAction::Children duplicatedChildren;
        if (!currentChildren.empty()) {
            try {
                duplicatedChildren = CameraResizeAction::Children(currentChildren.begin(), currentChildren.end());
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
        }
        children = duplicatedChildren;
        oldChildTransforms.clear();
        childWorldTransforms.clear();
        const auto capturedChildren = children;
        for (auto child : capturedChildren) {
            actions::node::detail::requireActionObject(child);
            auto& previousMap = oldChildTransforms;
            const auto previousKey = child->getUUID();
            const auto previousValue = child->localTransform;
            previousMap[previousKey] = previousValue;
            auto& worldMap = childWorldTransforms;
            const auto worldKey = child->getUUID();
            const auto worldValue = child->transform();
            worldMap[worldKey] = worldValue;
        }
    }
    void applyScale(NodePtr node, Vec2 scale) override {
        const auto receiver=weak_from_this().lock();
        const auto value = camera(node);
        ensureActive(value);
        actions::node::detail::requireActionObject(value);
        const auto oldScale = oldTransform.scale;
        const Vec2 ratio{scale.x / nonZeroScale(oldScale.x), scale.y / nonZeroScale(oldScale.y)};
        value->localTransform = oldTransform;
        value->localTransform.scale = Vec2{scaleSign(scale.x) * std::abs(oldScale.x),
            scaleSign(scale.y) * std::abs(oldScale.y)};
        value->localTransform.update();
        value->setViewport(Vec2{oldViewport.x * std::abs(ratio.x), oldViewport.y * std::abs(ratio.y)});
        value->transformChanged();
        const auto capturedChildren = children;
        for (auto child : capturedChildren) {
            actions::node::detail::requireActionObject(child);
            const auto world = childWorldTransforms.find(child->getUUID());
            if (world != childWorldTransforms.end()) {
                restoreWorldTransformUnder(child, value, world->second);
                child->notifyChange(child, NotifyReason::AttributeChanged);
                child->notifyChange(child, NotifyReason::Transformed);
            }
        }
        value->notifyChange(value, NotifyReason::AttributeChanged);
    }
    void endScale(NodePtr node, Vec2, ActionMap& result) override {
        const auto receiver=weak_from_this().lock();
        const auto value = camera(node);
        ensureActive(value);
        actions::node::detail::requireActionObject(value);
        if (value->localTransform.scale.x == oldTransform.scale.x &&
            value->localTransform.scale.y == oldTransform.scale.y) {
            const auto viewport = value->getViewport();
            if (viewport.x == oldViewport.x && viewport.y == oldViewport.y) return;
        }
        TransformMap newTransforms;
        const auto capturedChildren = children;
        for (auto child : capturedChildren) {
            actions::node::detail::requireActionObject(child);
            auto& currentMap = newTransforms;
            const auto key = child->getUUID();
            const auto transform = child->localTransform;
            currentMap[key] = transform;
        }
        auto& output=result;
        const auto key=compat::RetainedString::fromStaticLiteral("CameraResize");
        const auto action=CameraResizeAction::createSource([&] {
            return std::tuple{value,oldTransform,oldViewport,value->localTransform,
                value->getViewport(),children,oldChildTransforms,newTransforms};
        });
        output[key]=action;
        activeCamera.reset();
    }
};
} // namespace
} // namespace nijigenerate::viewport::common::transformhandle

namespace nicxlive::core::d_runtime {
// These are the three actual module-private Source classes, not aliases of
// the public base. Their defining TU retains their complete native identities.
#define NG_PRIVATE_TRANSFORM_ADAPTER_ORIGIN(Type, Qualified, Unqualified) \
    template<> struct SourceTypeSpellings< \
            ::nijigenerate::viewport::common::transformhandle::Type> { \
        inline static constexpr bool isNativeObject=true; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_PRIVATE_TRANSFORM_ADAPTER_ORIGIN(DefaultViewportTransformHandleAdapter,
    "nijigenerate.viewport.common.transformhandle.DefaultViewportTransformHandleAdapter",
    "DefaultViewportTransformHandleAdapter");
NG_PRIVATE_TRANSFORM_ADAPTER_ORIGIN(NodeOriginViewportTransformHandleAdapter,
    "nijigenerate.viewport.common.transformhandle.NodeOriginViewportTransformHandleAdapter",
    "NodeOriginViewportTransformHandleAdapter");
NG_PRIVATE_TRANSFORM_ADAPTER_ORIGIN(CameraViewportTransformHandleAdapter,
    "nijigenerate.viewport.common.transformhandle.CameraViewportTransformHandleAdapter",
    "CameraViewportTransformHandleAdapter");
#undef NG_PRIVATE_TRANSFORM_ADAPTER_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::viewport::common::transformhandle {
namespace {
void prepareAdapterClassInfo() {
    static const bool registered = [] {
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(ViewportTransformHandleAdapter),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.transformhandle.ViewportTransformHandleAdapter"));
        ngRegisterObjectClassName(typeid(DefaultViewportTransformHandleAdapter),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.transformhandle.DefaultViewportTransformHandleAdapter"));
        ngRegisterObjectClassName(typeid(NodeOriginViewportTransformHandleAdapter),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.transformhandle.NodeOriginViewportTransformHandleAdapter"));
        ngRegisterObjectClassName(typeid(CameraViewportTransformHandleAdapter),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.transformhandle.CameraViewportTransformHandleAdapter"));
        return true;
    }();
    (void)registered;
}
thread_local AdapterPtr defaultAdapter;
thread_local AdapterPtr nodeOriginAdapter;
thread_local AdapterPtr cameraAdapter;
thread_local NodePtr lastAdapterNode;
thread_local AdapterPtr lastAdapter;
AdapterPtr createAdapter(NodePtr node) {
    if (!defaultAdapter) {
        defaultAdapter = std::allocate_shared<DefaultViewportTransformHandleAdapter>(
            nicxlive::core::d_runtime::SourceAllocator<DefaultViewportTransformHandleAdapter>{});
        nodeOriginAdapter = std::allocate_shared<NodeOriginViewportTransformHandleAdapter>(
            nicxlive::core::d_runtime::SourceAllocator<NodeOriginViewportTransformHandleAdapter>{});
        cameraAdapter = std::allocate_shared<CameraViewportTransformHandleAdapter>(
            nicxlive::core::d_runtime::SourceAllocator<CameraViewportTransformHandleAdapter>{});
    }
    if (std::dynamic_pointer_cast<ExCamera>(node)) return cameraAdapter;
    if (std::dynamic_pointer_cast<ExDepthBone>(node) || std::dynamic_pointer_cast<ExDepthRigRoot>(node))
        return nodeOriginAdapter;
    return defaultAdapter;
}
}
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString ViewportTransformHandleAdapter::toString() {
    const auto receiver=weak_from_this().lock();
    prepareAdapterClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ViewportTransformHandleAdapter::toHash() {
    const auto receiver=weak_from_this().lock();
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ViewportTransformHandleAdapter::opEquals(std::shared_ptr<ViewportTransformHandleAdapter> other) {
    const auto receiver=weak_from_this().lock();
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int ViewportTransformHandleAdapter::opCmp(std::shared_ptr<ViewportTransformHandleAdapter>) {
    const auto receiver=weak_from_this().lock();
    prepareAdapterClassInfo();
    const auto className=nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message=[&] {
        try {
            std::string text="need opCmp for class ";
            text+=className.view();
            return compat::RetainedString(std::move(text));
        } catch(const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch(const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngViewportTransformHandleAdapterObjectEquals(std::shared_ptr<ViewportTransformHandleAdapter> left,
    std::shared_ptr<ViewportTransformHandleAdapter> right) {
    return nicxlive::core::common::ngClassObjectEquals(
        std::move(left), std::move(right), &prepareAdapterClassInfo);
}
std::shared_ptr<ViewportTransformHandleAdapter> ngViewportTransformHandleAdapter(NodePtr node) {
    if (node == lastAdapterNode && lastAdapter) return lastAdapter;
    lastAdapterNode = node;
    lastAdapter = createAdapter(node);
    return lastAdapter;
}
}
