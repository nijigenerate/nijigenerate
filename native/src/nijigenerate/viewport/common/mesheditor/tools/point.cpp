// Copyright 2020-2024 Inochi2D Project; 2025 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "point.hpp"
#include <nijigenerate/actions/vertex.hpp>
#include <nijigenerate/actions/mesh.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::point {
using namespace actions::vertex;
using namespace actions::mesh;
namespace input = core::input;
namespace stack = core::actionstack;
namespace {
template<class Operation>
decltype(auto) allocatePointStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
constexpr std::uint64_t noIndex = UINT64_MAX;
using Vec2 = base::Vec2;
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 subtract(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
template<class Callback>
base::Editor::MirrorVisitor mirrorVisitor(Callback callback) {
    return allocatePointStorage([&] {
        return std::make_shared<const std::function<void(std::uint32_t)>>(std::move(callback));
    });
}
core::math::mesh::MeshVertex* vertexAt(const core::math::mesh::MeshVertexArray& values,
    std::size_t index, std::size_t sourceLine) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/point.d"), sourceLine);
    return values[index];
}
template<std::size_t N>
compat::RetainedString text(const char (&key)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
}
template<std::size_t A, std::size_t K>
void status(const char (&action)[A], const char (&key)[K]) {
    const auto actionText = text(action);
    const auto keyText = text(key);
    widgets::ngStatusTooltip(actionText, keyText);
}
actions::ActionText name(base::EditorRef editor) {
    const auto target = compat::ngRequireObject(editor)->getTarget();
    return compat::RetainedString(compat::ngRequireObject(target)->getName());
}
Vec2 scaledDelta(ImGuiIO* io, Vec2 delta) {
    const float magnitude = compat::ngRequireObject(io)->KeyAlt ? 1.f : compat::ngRequireObject(io)->KeyShift ? 100.f : 10.f;
    return {delta.x * magnitude, delta.y * magnitude};
}
}
bool PointTool::onDragStart(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (compat::ngRequireObject(editor)->deformOnly) return NodeSelect::onDragStart(mouse, editor);
    if (!compat::ngRequireObject(editor)->isSelecting && !isDragging) {
        isDragging = true;
        action = VertexMoveAction::createSource([&] {
            return std::tuple{name(editor), editor, VertexUpdate{}};
        });
        return true;
    }
    return false;
}
bool PointTool::onDragEnd(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (!compat::ngRequireObject(editor)->deformOnly && action) {
        const auto current = action;
        if (auto mesh = std::dynamic_pointer_cast<MeshAction>(current)) {
            if (mesh->dirty) { mesh->updateNewState(); stack::incActionPush(action); }
        } else if (auto vertex = std::dynamic_pointer_cast<VertexAction>(current)) {
            if (vertex->dirty) { vertex->updateNewState(); stack::incActionPush(action); }
        }
        action.reset();
    }
    return NodeSelect::onDragEnd(mouse, editor);
}
bool PointTool::onDragUpdate(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (compat::ngRequireObject(editor)->deformOnly) return NodeSelect::onDragUpdate(mouse, editor);
    if (!isDragging) return false;
    if (auto move = std::dynamic_pointer_cast<VertexMoveAction>(action)) {
        const auto selection = compat::ngRequireObject(editor)->selected;
        // D closures share the foreach variable's location across iterations.
        const auto index = std::allocate_shared<std::uint64_t>(
            nicxlive::core::d_runtime::SourceAllocator<std::uint64_t>{});
        for (auto selected : selection) {
            *index = selected;
            compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, move, mouse, index](std::uint32_t axis) {
                const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({compat::ngRequireObject(editor)->mirrorVertex(axis, *index)});
                if (auto vertex = vertexAt(vertices, 0, 71))
                    move->moveVertex(vertex, add(vertex->position,
                        compat::ngRequireObject(editor)->mirror(axis, subtract(mouse, compat::ngRequireObject(editor)->lastMousePos))));
            }));
        }
    }
    if (!compat::ngRequireObject(editor)->selected.empty()) compat::ngRequireObject(editor)->maybeSelectOne = noIndex;
    compat::ngRequireObject(editor)->refreshMesh();
    return true;
}
bool PointTool::updateMeshEdit(ImGuiIO* io, EditorRef editor, bool& changed) {
    const auto keepAlive = shared_from_this();
    changed = false;
    status("Select", "Left Mouse");
    status("Create", "Ctrl+Left Mouse");
    if (input::ngInputIsMouseReleased(ImGuiMouseButton_Left)) onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) compat::ngRequireObject(editor)->maybeSelectOne = noIndex;
    const auto drawable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDrawable>(editor);
    const auto deformable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDeformable>(editor);
#ifndef NDEBUG
    if (!drawable && !deformable) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/point.d"), 113);
#endif
    const auto addOrRemove = [&](auto concrete, bool selectedOnly) {
        const auto vertexCount = [&] {
            if constexpr (requires { compat::ngRequireObject(concrete)->vertices(); }) return compat::ngRequireObject(concrete)->vertices().size();
            else return compat::ngRequireObject(concrete)->vertices.size();
        };
        const auto hit = compat::ngRequireObject(editor)->getVerticesByIndex({compat::ngRequireObject(editor)->vtxAtMouse});
        if (vertexAt(hit, 0, 117)) {
            const auto removal = VertexRemoveAction::createSource([&] {
                return std::tuple{name(editor), editor, VertexUpdate{}};
            });
            if (!selectedOnly || compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse)) {
                const auto removing = std::allocate_shared<core::math::mesh::MeshVertexArray>(
                    nicxlive::core::d_runtime::SourceAllocator<core::math::mesh::MeshVertexArray>{});
                compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, removing](std::uint32_t axis) {
                    const auto index = compat::ngRequireObject(editor)->getVertexFromPoint(compat::ngRequireObject(editor)->mirror(axis, compat::ngRequireObject(editor)->mousePos));
                    const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({index});
                    if (auto* vertex = vertexAt(vertices, 0, 125)) removing->append(vertex);
                }));
                for (auto vertex : *removing) removal->removeVertex(vertex);
                compat::ngRequireObject(editor)->refreshMesh(); compat::ngRequireObject(editor)->vertexMapDirty = true; compat::ngRequireObject(editor)->selected.resize(0);
                compat::ngRequireObject(editor)->updateMirrorSelected(); compat::ngRequireObject(editor)->maybeSelectOne = noIndex; compat::ngRequireObject(editor)->vtxAtMouse = noIndex;
                changed = true;
            }
            removal->updateNewState(); stack::incActionPush(removal);
        } else {
            const auto insertion = VertexAddAction::createSource([&] {
                return std::tuple{name(editor), editor, VertexUpdate{}};
            });
            const auto vertex = std::allocate_shared<core::math::mesh::MeshVertexHandle>(
                nicxlive::core::d_runtime::SourceAllocator<core::math::mesh::MeshVertexHandle>{});
            const auto offset = vertexCount();
            if (compat::ngRequireObject(editor)->isOnMirror(compat::ngRequireObject(editor)->mousePos, compat::ngRequireObject(editor)->meshEditAOE)) {
                compat::ngRequireObject(editor)->placeOnMirror(compat::ngRequireObject(editor)->mousePos, compat::ngRequireObject(editor)->meshEditAOE);
            } else compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, insertion, vertex](std::uint32_t axis) {
                *vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
                    return compat::ngRequireObject(editor)->mirror(axis, compat::ngRequireObject(editor)->mousePos);
                });
                insertion->addVertex(vertex->get());
            }));
            compat::ngRequireObject(editor)->refreshMesh(); compat::ngRequireObject(editor)->vertexMapDirty = true;
            compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(io)->KeyCtrl ? vertexCount() - 1 : offset);
            insertion->updateNewState(); stack::incActionPush(insertion); changed = true;
            if (drawable) {
                const auto mesh = drawable->getMesh();
                const auto connect = [&] {
                    if (!*vertex) return;
                    const auto previous = stack::incActionFindLast<VertexAddAction>(3);
                    if (!previous || previous->vertices.empty()) return;
                    const auto previousIndex = compat::ngRequireObject(editor)->getVertexFromPoint(compat::ngRequireObject(
                        previous->vertices[previous->vertices.size() - 1].vertex)->position);
                    const auto previousVertices = compat::ngRequireObject(editor)->getVerticesByIndex({previousIndex});
                    auto* previousVertex = vertexAt(previousVertices, 0, 179);
                    if (!previousVertex) return;
                    const auto connection = MeshConnectAction::createSource([&] {
                        return std::tuple{name(editor), editor, mesh, MeshUpdate{}};
                    });
                    const auto previousOwner = core::math::mesh::ngTryRetainMeshVertex(previousVertex);
                    compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor(
                        [editor, previousVertex, previousOwner, connection, vertex](std::uint32_t axis) {
                        (void)previousOwner;
                        auto* first = compat::ngRequireObject(editor)->mirrorVertex(axis, previousVertex);
                        [[maybe_unused]] const auto firstOwner = core::math::mesh::ngTryRetainMeshVertex(first);
                        auto* second = compat::ngRequireObject(editor)->mirrorVertex(axis, vertex->get());
                        [[maybe_unused]] const auto secondOwner = core::math::mesh::ngTryRetainMeshVertex(second);
                        if (first && second) connection->connect(first, second);
                    }));
                    compat::ngRequireObject(editor)->refreshMesh(); connection->updateNewState(); stack::incActionPush(connection); changed = true;
                };
                if (autoConnect) connect();
            }
        }
    };
    if (input::ngInputIsKeyPressed(ImGuiKey_Delete)) {
        const auto removal = VertexRemoveAction::createSource([&] {
            return std::tuple{name(editor), editor, VertexUpdate{}};
        });
        compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, removal](std::uint32_t axis) {
            const auto selection = compat::ngRequireObject(editor)->selected;
            for (auto index : selection) {
                const auto mirrored = compat::ngRequireObject(editor)->mirrorVertex(axis, index);
                if (mirrored != noIndex) {
                    const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({mirrored});
                    removal->removeVertex(vertexAt(vertices, 0, 211), false);
                }
            }
        }));
        removal->removeVertices(); removal->updateNewState(); stack::incActionPush(removal);
        compat::ngRequireObject(editor)->selected = {}; compat::ngRequireObject(editor)->updateMirrorSelected(); compat::ngRequireObject(editor)->refreshMesh();
        compat::ngRequireObject(editor)->vertexMapDirty = true; changed = true;
    }
    // foreachMirror is virtual and accepts an escaping delegate in the original.
    // Retain the shared closure variable rather than a reference to this stack frame.
    const auto move = std::allocate_shared<std::shared_ptr<VertexMoveAction>>(
        nicxlive::core::d_runtime::SourceAllocator<std::shared_ptr<VertexMoveAction>>{});
    const auto shift = [&](Vec2 delta) {
        delta = scaledDelta(io, delta);
        compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, delta, move](std::uint32_t axis) {
            const auto mirroredDelta = compat::ngRequireObject(editor)->mirrorDelta(axis, delta);
            const auto selection = compat::ngRequireObject(editor)->selected;
            for (auto index : selection) {
                const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({compat::ngRequireObject(editor)->mirrorVertex(axis, index)});
                if (auto vertex = vertexAt(vertices, 0, 238)) {
                    if (!*move) *move = VertexMoveAction::createSource([&] {
                        return std::tuple{name(editor), editor, VertexUpdate{}};
                    });
                    (*move)->moveVertex(vertex, add(vertex->position, mirroredDelta));
                }
            }
        }));
        compat::ngRequireObject(editor)->refreshMesh(); changed = true;
    };
    if (input::ngInputIsKeyPressed(ImGuiKey_LeftArrow)) shift({-1, 0});
    else if (input::ngInputIsKeyPressed(ImGuiKey_RightArrow)) shift({1, 0});
    else if (input::ngInputIsKeyPressed(ImGuiKey_DownArrow)) shift({0, 1});
    else if (input::ngInputIsKeyPressed(ImGuiKey_UpArrow)) shift({0, -1});
    if (*move) { (*move)->updateNewState(); stack::incActionPush(*move); }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (compat::ngRequireObject(io)->KeyCtrl && !compat::ngRequireObject(io)->KeyShift) {
            if (drawable) addOrRemove(drawable, false); else addOrRemove(deformable, false);
        } else if (compat::ngRequireObject(editor)->isPointOver(compat::ngRequireObject(editor)->mousePos)) {
            if (compat::ngRequireObject(io)->KeyShift) compat::ngRequireObject(editor)->toggleSelect(compat::ngRequireObject(editor)->vtxAtMouse);
            else if (!compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse)) compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->vtxAtMouse);
            else compat::ngRequireObject(editor)->maybeSelectOne = compat::ngRequireObject(editor)->vtxAtMouse;
        } else { compat::ngRequireObject(editor)->selectOrigin = compat::ngRequireObject(editor)->mousePos; compat::ngRequireObject(editor)->isSelecting = true; }
    }
    if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && input::ngInputIsMouseReleased(ImGuiMouseButton_Left) && compat::ngRequireObject(editor)->maybeSelectOne != noIndex)
        compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !compat::ngRequireObject(io)->KeyShift && !compat::ngRequireObject(io)->KeyCtrl) {
        if (drawable) addOrRemove(drawable, true); else addOrRemove(deformable, true);
    }
    if (viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        input::ngInputIsDragRequested(ImGuiMouseButton_Left)) onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor);
    return true;
}
std::int32_t PointTool::peekDeformEdit(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (input::ngInputIsMouseReleased(ImGuiMouseButton_Left)) onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) compat::ngRequireObject(editor)->maybeSelectOne = noIndex;
    if (input::ngInputIsKeyPressed(ImGuiKey_LeftArrow)) return TranslateLeft;
    if (input::ngInputIsKeyPressed(ImGuiKey_RightArrow)) return TranslateRight;
    if (input::ngInputIsKeyPressed(ImGuiKey_DownArrow)) return TranslateDown;
    if (input::ngInputIsKeyPressed(ImGuiKey_UpArrow)) return TranslateUp;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (compat::ngRequireObject(editor)->isPointOver(compat::ngRequireObject(editor)->mousePos)) {
            if (compat::ngRequireObject(io)->KeyShift) return int(SelectActionID::ToggleSelect);
            if (!compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse)) return int(SelectActionID::SelectOne);
            return int(SelectActionID::MaybeSelectOne);
        }
        return int(SelectActionID::SelectArea);
    }
    if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && input::ngInputIsMouseReleased(ImGuiMouseButton_Left) && compat::ngRequireObject(editor)->maybeSelectOne != noIndex)
        return int(SelectActionID::SelectMaybeSelectOne);
    if (isDragging) return Translate;
    if (viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        input::ngInputIsDragRequested(ImGuiMouseButton_Left) && !compat::ngRequireObject(editor)->isSelecting) return int(SelectActionID::StartDrag);
    return int(SelectActionID::None);
}
bool PointTool::updateDeformEdit(ImGuiIO* io, EditorRef editor, std::int32_t requested, bool& changed) {
    const auto keepAlive = shared_from_this(); changed = false;
    status("Select", "Left Mouse");
    const auto keyboardMoved = std::allocate_shared<bool>(
        nicxlive::core::d_runtime::SourceAllocator<bool>{}, false);
    const auto shift = [&](Vec2 delta) {
        delta = scaledDelta(io, delta);
        compat::ngRequireObject(editor)->foreachMirror(mirrorVisitor([editor, delta, keyboardMoved](std::uint32_t axis) {
            const auto mirroredDelta = compat::ngRequireObject(editor)->mirrorDelta(axis, delta);
            const auto selection = compat::ngRequireObject(editor)->selected;
            for (auto index : selection) {
                const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({index, compat::ngRequireObject(editor)->mirrorVertex(axis, index)});
                auto* vertex = vertexAt(vertices, 0, 372);
                auto* mirrored = vertexAt(vertices, 1, 373);
                compat::ngRequireObject(editor)->getDeformAction(); compat::ngRequireObject(editor)->updateAddVertexAction(vertex); compat::ngRequireObject(editor)->markActionDirty();
                *keyboardMoved = true;
                if (mirrored) mirrored->position = add(mirrored->position, mirroredDelta);
            }
        }));
        compat::ngRequireObject(editor)->refreshMesh(); changed = true;
    };
    if (requested == TranslateLeft) shift({-1, 0});
    else if (requested == TranslateRight) shift({1, 0});
    else if (requested == TranslateDown) shift({0, 1});
    else if (requested == TranslateUp) shift({0, -1});
    if (*keyboardMoved) compat::ngRequireObject(editor)->pushDeformAction();
    if (requested == int(SelectActionID::ToggleSelect)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != noIndex) compat::ngRequireObject(editor)->toggleSelect(compat::ngRequireObject(editor)->vtxAtMouse);
    } else if (requested == int(SelectActionID::SelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != noIndex) compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->vtxAtMouse); else compat::ngRequireObject(editor)->deselectAll();
    } else if (requested == int(SelectActionID::MaybeSelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != noIndex) compat::ngRequireObject(editor)->maybeSelectOne = compat::ngRequireObject(editor)->vtxAtMouse;
    } else if (requested == int(SelectActionID::SelectArea)) {
        compat::ngRequireObject(editor)->selectOrigin = compat::ngRequireObject(editor)->mousePos; compat::ngRequireObject(editor)->isSelecting = true;
    }
    if (requested == int(SelectActionID::SelectMaybeSelectOne) && compat::ngRequireObject(editor)->maybeSelectOne != noIndex) compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
    if (requested == int(SelectActionID::StartDrag) && onDragStart(compat::ngRequireObject(editor)->mousePos, editor))
        changed = onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor) || changed;
    if (requested == Translate) changed = onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor) || changed;
    return true;
}
std::int32_t PointTool::peek(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this(); NodeSelect::peek(io, editor);
    return compat::ngRequireObject(editor)->deformOnly ? peekDeformEdit(io, editor) : 0;
}
std::int32_t PointTool::unify(compat::Slice<std::int32_t> requests) {
    struct PriorityHash {
        std::size_t operator()(std::int32_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    nicxlive::core::common::ResourceKeyMap<std::int32_t, std::int32_t,
        PriorityHash, std::equal_to<std::int32_t>> priorities;
    priorities[Add] = 2;
    priorities[Remove] = 2;
    priorities[Translate] = 1;
    priorities[TranslateUp] = 0;
    priorities[TranslateDown] = 0;
    priorities[TranslateLeft] = 0;
    priorities[TranslateRight] = 0;
    priorities[int(SelectActionID::None)] = 10;
    priorities[int(SelectActionID::SelectArea)] = 5;
    priorities[int(SelectActionID::ToggleSelect)] = 2;
    priorities[int(SelectActionID::SelectOne)] = 2;
    priorities[int(SelectActionID::MaybeSelectOne)] = 2;
    priorities[int(SelectActionID::StartDrag)] = 2;
    priorities[int(SelectActionID::SelectMaybeSelectOne)] = 2;
    const auto priority = [&](std::int32_t value) {
        const auto found = priorities.find(value);
        if (found == priorities.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/point.d"), 458);
        return found->second;
    };
    auto selected = int(SelectActionID::None); auto current = priority(selected);
    for (auto value : requests) if (const auto next = priority(value); next < current) { current = next; selected = value; }
    return selected;
}
bool PointTool::update(ImGuiIO* io, EditorRef editor, std::int32_t requested, bool& changed) {
    const auto keepAlive = shared_from_this(); NodeSelect::update(io, editor, requested, changed);
    if (compat::ngRequireObject(editor)->deformOnly) updateDeformEdit(io, editor, requested, changed); else updateMeshEdit(io, editor, changed);
    return changed;
}
bool detail::displayOptions(bool deformOnly, base::Editors editors) {
    if (!deformOnly) {
        const auto tool = [&] {
            if (editors.empty()) return std::shared_ptr<PointTool>{};
            const auto values = allocatePointStorage([&] { return editors.values(); });
            return std::dynamic_pointer_cast<PointTool>(compat::ngRequireObject(values[0])->getTool());
        }();
        ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\x9d\x86", ImVec2(0, 0),
                tool && !tool->isAutoConnect() ? ImVec4(.6f, .6f, .6f, 1) : widgets::colorUndefined)) {
            if (!editors.isNull()) for (const auto& entry : editors) {
                const auto editor = entry.second;
                if (const auto point = std::dynamic_pointer_cast<PointTool>(compat::ngRequireObject(editor)->getTool()))
                    point->setAutoConnect(!point->isAutoConnect());
            }
        }
        widgets::ngTooltip(text("Auto connect vertices")); ImGui::EndGroup();
    }
    return false;
}
}
