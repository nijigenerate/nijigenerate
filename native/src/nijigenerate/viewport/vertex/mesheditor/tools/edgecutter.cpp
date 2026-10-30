#include "edgecutter.hpp"
#include <nijigenerate/actions/mesh.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <math.hpp>
#include <functional>
#include <new>
#include <source_location>

namespace nijigenerate::viewport::vertex::mesheditor::tools::edgecutter {
bool EdgeCutterTool::onDragEnd(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = weak_from_this().lock();
    if (!isDragging) return false;
    isDragging = false; dragEnd = mouse;
    const auto action = compat::ngRequireObject(editor)->newMeshDisconnectAction();
    const auto a = dragOrigin, b = dragEnd;
    using Vertex = core::math::mesh::MeshVertex;
    using EdgeCallback = std::function<void(Vertex*, Vertex*)>;
    std::shared_ptr<const EdgeCallback> callback;
    try {
        callback = std::make_shared<const EdgeCallback>(
            [action, a, b](auto* vertex, auto* connection) {
                const Vec2 position = compat::ngRequireObject(vertex)->position;
                const Vec2 otherPosition = compat::ngRequireObject(connection)->position;
                if (nicxlive::core::math::areLineSegmentsIntersecting(a, b, position, otherPosition))
                    compat::ngRequireObject(action)->disconnect(vertex, connection);
            });
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    compat::ngRequireObject(editor)->forEachEdge(callback);
    compat::ngRequireObject(action)->updateNewState(); core::actionstack::incActionPush(action);
    compat::ngRequireObject(editor)->refreshMesh();
    return true;
}
bool EdgeCutterTool::update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) {
    const auto keepAlive = weak_from_this().lock();
    changed = false;
    NodeSelect::update(io, editor, action, changed);
    const auto status = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Cut edges intersecting the line"));
    const auto key = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Drag Left Mouse"));
    widgets::ngStatusTooltip(status, key);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor);
    if (isDragging && core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left))
        onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    return true;
}
void EdgeCutterTool::draw(toolbase::CameraRef camera, EditorRef editor) {
    const auto keepAlive = weak_from_this().lock();
    NodeSelect::draw(camera, editor);
    if (!isDragging) return;
    nicxlive::core::math::Vec3Array lines{
        {dragOrigin.x, dragOrigin.y, 0}, {dragEnd.x, dragEnd.y, 0}};
    core::dbg::inDbgSetBuffer(lines); core::dbg::inDbgLineWidth(3);
    core::dbg::inDbgDrawLines({0, 0, 0, 1}, nicxlive::core::math::Mat4::identity());
    core::dbg::inDbgDrawLines({1, 0, 0, 1}, nicxlive::core::math::Mat4::identity());
}
}
