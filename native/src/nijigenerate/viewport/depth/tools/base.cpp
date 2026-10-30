#include "base.hpp"
#include "nijigenerate/core/input_mouse_position.hpp"
#include "nijigenerate/viewport/depth/mesheditor/node.hpp"
#include "nijigenerate/viewport/depth/mesheditor/editor.hpp"
#include "nijigenerate/viewport/depth/viewport.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::viewport::depth::tools::base {

namespace {
void prepareDepthToolClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<DepthEditTool>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        ngRegisterObjectTypeNames<DepthEditTool>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        ngRegisterObjectClassName(typeid(DepthEditTool), name);
        return true;
    }();
    (void)registered;
}
}

// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthEditTool::toString() {
    [[maybe_unused]] const auto receiverLifetime = sourceOwner_.lock();
    prepareDepthToolClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthEditTool::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthEditTool::opEquals(std::shared_ptr<DepthEditTool> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthEditTool::opCmp(std::shared_ptr<DepthEditTool>) {
    [[maybe_unused]] const auto receiverLifetime = sourceOwner_.lock();
    prepareDepthToolClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDepthToolObjectEquals(std::shared_ptr<DepthEditTool> left, std::shared_ptr<DepthEditTool> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareDepthToolClassInfo);
}

void DepthEditTool::selected(std::shared_ptr<viewport::DepthEditViewport>) {}

void DepthEditTool::draw(std::shared_ptr<nicxlive::core::math::Camera>,
    std::shared_ptr<viewport::DepthEditViewport>) {}

void DepthEditTool::drawOptions(std::shared_ptr<viewport::DepthEditViewport>) {}

bool DepthEditTool::update(ImGuiIO*, std::shared_ptr<nicxlive::core::math::Camera>,
    std::shared_ptr<viewport::DepthEditViewport>) {
    return false;
}

nicxlive::core::math::Vec2 DepthEditTool::screenMouseToView(
    ImGuiIO*, std::shared_ptr<nicxlive::core::math::Camera>) {
    const auto point = core::input::ngInputGetMousePosition();
    return {-point.x, -point.y};
}

nicxlive::core::math::Vec2 DepthEditTool::modelToView(
    std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
    nicxlive::core::math::Vec2 point, float depth, std::shared_ptr<viewport::DepthEditViewport> viewport) {
    const auto receiverLifetime = sourceOwner_.lock();
    const auto receiver = compat::ngRequireObject(editor);
    auto& camera = compat::ngRequireObject(viewport)->depthCameraState();
    return receiver->modelToDepthView(point, depth, camera);
}

nicxlive::core::math::Vec2 DepthEditTool::viewToModel(
    std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
    nicxlive::core::math::Vec2 point, std::shared_ptr<viewport::DepthEditViewport> viewport, float depth) {
    const auto receiverLifetime = sourceOwner_.lock();
    const auto receiver = compat::ngRequireObject(editor);
    auto& camera = compat::ngRequireObject(viewport)->depthCameraState();
    return receiver->depthViewToModel(point, camera, depth);
}

nicxlive::core::math::Vec2 DepthEditTool::screenMouseToModel(ImGuiIO* io,
    std::shared_ptr<nicxlive::core::math::Camera> camera,
    std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
    std::shared_ptr<viewport::DepthEditViewport> viewport, float depth) {
    const auto receiverLifetime = sourceOwner_.lock();
    const auto point = screenMouseToView(io, camera);
    return viewToModel(editor, point, viewport, depth);
}

std::shared_ptr<mesheditor::node::DepthMeshEditorOne> DepthEditTool::nearestVertexFromScreenMouse(
    ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
    std::shared_ptr<viewport::DepthEditViewport> viewport, float radius, std::ptrdiff_t& index) {
    const auto receiverLifetime = sourceOwner_.lock();
    index = 0;
    const auto editorSet = compat::ngRequireObject(viewport)->getEditor();
    if (!editorSet) return nullptr;
    const auto point = screenMouseToView(io, camera);
    return editorSet->findNearestProjectedVertex(point, radius, index);
}

std::shared_ptr<mesheditor::node::DepthMeshEditorOne> DepthEditTool::nearestVertexFromScreenMouse(
    ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
    std::shared_ptr<viewport::DepthEditViewport> viewport, std::ptrdiff_t& index) {
    const auto receiverLifetime = sourceOwner_.lock();
    index = 0;
    return nearestVertexFromScreenMouse(io, camera, viewport, std::numeric_limits<float>::max(), index);
}

} // namespace nijigenerate::viewport::depth::tools::base
