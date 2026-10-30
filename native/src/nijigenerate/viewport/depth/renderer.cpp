#include "nijigenerate/viewport/depth/renderer.hpp"
#include "nijigenerate/viewport/depth/common/targetview.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <compat/object_metadata.hpp>
#include <string>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::depth {

namespace {
compat::RetainedString comparisonMessage(compat::RetainedString name) {
    try {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void prepareDepthRendererClassInfo() {
    static const bool registered = [] {
        const auto registerType = []<class T> {
            using namespace nicxlive::core::d_runtime;
            using Names = SourceObjectTypeNames<T>;
            const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
            ngRegisterObjectTypeNames<T>(name,
                compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
            ngRegisterObjectClassName(typeid(T), name);
        };
        registerType.template operator()<DepthTargetRenderer>();
        registerType.template operator()<DepthTextureMeshRenderer>();
        registerType.template operator()<DepthTargetOffscreenTextureRenderer>();
        return true;
    }();
    (void)registered;
}
}

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DepthTargetRenderer::prepareSourceClassInfo() { prepareDepthRendererClassInfo(); }
compat::RetainedString DepthTargetRenderer::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthTargetRenderer::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthTargetRenderer::opEquals(std::shared_ptr<DepthTargetRenderer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthTargetRenderer::opCmp(std::shared_ptr<DepthTargetRenderer>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void DepthTextureMeshRenderer::prepareSourceClassInfo() { prepareDepthRendererClassInfo(); }
compat::RetainedString DepthTextureMeshRenderer::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthTextureMeshRenderer::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthTextureMeshRenderer::opEquals(std::shared_ptr<DepthTextureMeshRenderer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthTextureMeshRenderer::opCmp(std::shared_ptr<DepthTextureMeshRenderer>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void DepthTargetOffscreenTextureRenderer::prepareSourceClassInfo() { prepareDepthRendererClassInfo(); }
compat::RetainedString DepthTargetOffscreenTextureRenderer::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthTargetOffscreenTextureRenderer::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthTargetOffscreenTextureRenderer::opEquals(std::shared_ptr<DepthTargetOffscreenTextureRenderer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthTargetOffscreenTextureRenderer::opCmp(std::shared_ptr<DepthTargetOffscreenTextureRenderer>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    throw compat::ThrowableException(comparisonMessage(name),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

DepthTargetRenderer::~DepthTargetRenderer() = default;

DepthTargetRenderMesh DepthTargetRenderer::buildMesh(Target target, DepthCamera3D& depthCamera) {
    const auto receiver = weak_from_this().lock();
    DepthTargetRenderMesh mesh;
    if (!target) return mesh;
    const auto vertices = target->getVertices();
    const auto depths = target->copyWorkingDepths();
    // D evaluates these virtual getter arguments from left to right.
    const auto indices = target->getIndices();
    const auto minPoint = target->boundsMin();
    const auto maxPoint = target->boundsMax();
    const auto scale = target->depthDisplayScale();
    mesh = buildMesh(vertices, depths, indices, minPoint, maxPoint, scale, depthCamera);
    target->projectedPoints = mesh.positions.duplicate();
    return mesh;
}

compat::Slice<DepthTargetRenderLine> DepthTargetRenderer::buildGridLines(Target target, DepthCamera3D& depthCamera) {
    const auto receiver = weak_from_this().lock();
    if (!target) return {};
    const auto mesh = buildMesh(target, depthCamera);
    const auto vertices = target->getVertices();
    return buildGridLines(vertices, mesh.positions);
}

} // namespace nijigenerate::viewport::depth
