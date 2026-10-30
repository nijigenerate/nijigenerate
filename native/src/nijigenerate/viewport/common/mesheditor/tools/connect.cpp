#include "connect.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/actions/mesh.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/math/mesh_vertices.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <utility>
#include <tuple>

namespace nijigenerate::viewport::common::mesheditor::tools::connect {
namespace {
template<class Function>
auto makeMirrorCallback(Function&& function) {
    try {
        return std::make_shared<const std::function<void(std::uint32_t)>>(
            std::forward<Function>(function));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
actions::ActionText actionName(base::EditorRef editor) {
    const auto target = compat::ngRequireObject(editor)->getTarget();
    return compat::RetainedString(compat::ngRequireObject(target)->getName());
}
std::uint64_t lastSelected(base::EditorRef editor, std::size_t sourceLine) {
    const auto selected = compat::ngRequireObject(editor)->selected;
    if (selected.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(selected.size() - 1, selected.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/connect.d"),
        sourceLine);
    return selected[selected.size() - 1];
}
core::math::mesh::MeshVertex* vertexAt(const core::math::mesh::MeshVertexArray& vertices, std::size_t index,
        std::size_t sourceLine) {
    if (index >= vertices.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, vertices.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/connect.d"),
        sourceLine);
    return vertices[index];
}
}
bool ConnectTool::updateMeshEdit(ImGuiIO* io, EditorRef editor, bool& changed) {
    changed = false;
    const auto keepAlive = shared_from_this();
    if (core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left))
        onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (!editor) compat::ngFailNullAccess(true);
        editor->maybeSelectOne = UINT64_MAX;
    }
    const auto tip = []<std::size_t N, std::size_t K>(const char (&text)[N], const char (&key)[K]) {
        const auto actionText = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
        const auto keyText = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        widgets::ngStatusTooltip(actionText, keyText);
    };
    if (compat::ngRequireObject(editor)->selected.empty()) tip("Select", "Left Mouse");
    else { tip("Connect/Disconnect", "Left Mouse"); tip("Connect Multiple", "Shift+Left Mouse"); }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) {
            const auto previousIndex = compat::ngRequireObject(editor)->selectOne(editor->vtxAtMouse);
            const auto previousHeld = compat::ngRequireObject(editor)->getVerticesByIndex({previousIndex});
            auto* previous = vertexAt(previousHeld, 0, 44);
            const auto lastHeld = compat::ngRequireObject(editor)->getVerticesByIndex({lastSelected(editor, 45)});
            auto* last = vertexAt(lastHeld, 0, 45);
            if (previous) {
                if (previousIndex != lastSelected(editor, 47)) {
                    auto drawable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDrawable>(editor);
                    const auto mesh = compat::ngRequireObject(drawable)->getMesh();
                    if (!core::math::mesh::isConnectedTo(previous, last)) {
                        const auto action = actions::mesh::MeshConnectAction::createSource([&] {
                            return std::tuple{actionName(editor), editor, mesh, actions::mesh::MeshUpdate{}};
                        });
                        compat::ngRequireObject(editor)->foreachMirror(
                            makeMirrorCallback(
                            [editor, action, previousIndex](std::uint32_t axis) {
                            const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({
                                editor->mirrorVertex(axis, previousIndex),
                                editor->mirrorVertex(axis, lastSelected(editor, 55))});
                            auto* previous = vertexAt(vertices, 0, 56);
                            auto* selected = vertexAt(vertices, 1, 57);
                            if (previous && selected) action->connect(previous, selected);
                        }));
                        action->updateNewState(); core::actionstack::incActionPush(action); changed = true;
                    } else {
                        const auto action = actions::mesh::MeshDisconnectAction::createSource([&] {
                            return std::tuple{actionName(editor), editor, mesh, actions::mesh::MeshUpdate{}};
                        });
                        compat::ngRequireObject(editor)->foreachMirror(
                            makeMirrorCallback(
                            [editor, action, previousIndex](std::uint32_t axis) {
                            const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({
                                editor->mirrorVertex(axis, previousIndex),
                                editor->mirrorVertex(axis, lastSelected(editor, 69))});
                            auto* previous = vertexAt(vertices, 0, 70);
                            auto* selected = vertexAt(vertices, 1, 71);
                            if (previous && selected) action->disconnect(previous, selected);
                        }));
                        action->updateNewState(); core::actionstack::incActionPush(action); changed = true;
                    }
                    if (!compat::ngRequireObject(io)->KeyShift) editor->deselectAll();
                } else compat::ngRequireObject(editor)->deselectAll();
            }
            compat::ngRequireObject(editor)->refreshMesh();
        } else compat::ngRequireObject(editor)->deselectAll();
    }
    return true;
}
}
