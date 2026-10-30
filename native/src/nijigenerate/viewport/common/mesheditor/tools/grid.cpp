#include "grid.hpp"
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/core/math/mesh_apply.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nodes/grid_axis_sort.hpp>
#include <bit>
#include <cmath>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools::grid {
using operations::impl::IncMeshEditorOneDrawable;
using operations::impl::IncMeshEditorOneDeformable;
using nicxlive::core::nodes::MeshData;
using nicxlive::core::nodes::GridDeformer;
namespace {
template<class Operation>
decltype(auto) allocateGridStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class Axis> std::ptrdiff_t findIndex(Axis axis, float value) {
    for (std::size_t i = 0; i < axis.size(); ++i) if (axis[i] == value)
        return std::bit_cast<std::ptrdiff_t>(i);
    return -1;
}
template<class Axis> std::int32_t find(Axis axis, float value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(findIndex(axis, value)));
}
template<class Axis> void sortAxis(Axis axis) {
    nicxlive::core::nodes::grid_axis_detail::sort({axis.data(), axis.size()});
}
float component(base::Vec2 value, std::int32_t index,std::size_t sourceLine) {
    if (index == 0) return value.x;
    if (index == 1) return value.y;
    throw compat::ngStaticError<compat::ArrayIndexError>(static_cast<std::size_t>(index),2,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/grid.d"),sourceLine);
}
std::int32_t otherAxis(std::int32_t axis) {
    return std::bit_cast<std::int32_t>(1u-std::bit_cast<std::uint32_t>(axis));
}
template<class Array> decltype(auto) sourceAt(Array& values,std::size_t index,std::size_t line) {
    if(index>=values.size())throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/grid.d"),line);
    return values[index];
}
}
GridTool::MeshRef GridTool::createEmptyMesh() {
    return common::mesh::IncMesh::create(std::allocate_shared<MeshData>(
        nicxlive::core::d_runtime::SourceAllocator<MeshData>{}));
}
GridTool::MeshRef GridTool::getMesh(EditorRef editor, bool& isVirtual) {
    const auto keepAlive = shared_from_this();
    isVirtual = false;
    if (const auto drawable = std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(editor)) return drawable->getMesh();
    if (const auto deformable = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor)) {
        isVirtual = true;
        const auto found = deformableMeshes.find(deformable);
        if (found != deformableMeshes.end()) return found->second.mesh;
        const auto grid = std::dynamic_pointer_cast<GridDeformer>(deformable->getTarget());
        if (!grid) return {};
        const auto mesh = common::mesh::ngCreateIncMesh(grid->getVertices());
        deformableMeshes[deformable] = {mesh};
        deformable->vertices = common::mesh::ngMeshVerticesFromPositions(common::mesh::ngMeshPositions(mesh));
        deformable->vertexMapDirty = true;
        deformable->refreshMesh();
        return mesh;
    }
    return {};
}
void GridTool::resetVirtualMeshAsEmpty(EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (const auto deformable = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor)) {
        const auto mesh = createEmptyMesh();
        deformableMeshes[deformable] = {mesh};
        deformable->vertices = {};
        deformable->vertexMapDirty = false;
        deformable->deselectAll();
        deformable->refreshMesh();
    }
}
void GridTool::refreshMeshView(EditorRef editor, MeshRef mesh) {
    const auto keepAlive = shared_from_this();
    if (const auto deformable = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor)) {
        deformable->vertices = common::mesh::ngMeshVerticesFromPositions(common::mesh::ngMeshPositions(mesh));
        deformable->vertexMapDirty = true;
        deformable->refreshMesh();
    } else compat::ngRequireObject(editor)->refreshMesh();
}
bool GridTool::applyVirtualMeshToTarget(EditorRef editor) {
    const auto keepAlive = shared_from_this();
    bool isVirtual = false;
    auto mesh = getMesh(editor, isVirtual);
    if (!isVirtual || !mesh) return false;
    if (compat::ngRequireObject(mesh)->vertices.empty() && compat::ngRequireObject(mesh)->axes.size() < 2) return false;
    if (const auto deformable = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor))
        if (const auto target = std::dynamic_pointer_cast<GridDeformer>(deformable->getTarget())) {
            core::math::mesh::applyMeshToTarget(target, compat::ngRequireObject(mesh)->vertices, &mesh);
            deformable->vertexMapDirty = false;
            return true;
        }
    return false;
}
bool GridTool::isOnGrid(MeshRef mesh, std::int32_t axis, Vec2 mouse, float threshold, float& value) {
    value = nan();
    if (compat::ngRequireObject(mesh)->axes.size() != 2) return false;
    const auto lowCoordinate=component(mouse,axis,144);
    const auto lowAxis=compat::ngRequireObject(mesh)->axes[otherAxis(axis)];
    if(!(lowCoordinate>=sourceAt(lowAxis,0,144)-threshold))return false;
    const auto highCoordinate=component(mouse,axis,144);
    const auto highAxis=compat::ngRequireObject(mesh)->axes[otherAxis(axis)];
    if (highCoordinate <= sourceAt(highAxis,highAxis.size()-1,144) + threshold) {
        const auto values = compat::ngRequireObject(mesh)->axes[axis];
        for (float candidate : values) if (std::abs(component(mouse,otherAxis(axis),146) - candidate) < threshold) {
            value = candidate; return true;
        }
    }
    return false;
}
bool GridTool::isOnEdge(MeshRef mesh, std::int32_t axis, Vec2 mouse, float threshold, float& value) {
    value = nan();
    if (compat::ngRequireObject(mesh)->axes.size() != 2) return false;
    const auto lowCoordinate=component(mouse,axis,157);
    const auto lowAxis=compat::ngRequireObject(mesh)->axes[otherAxis(axis)];
    if(!(lowCoordinate>=sourceAt(lowAxis,0,157)-threshold))return false;
    const auto highCoordinate=component(mouse,axis,157);
    const auto highAxis=compat::ngRequireObject(mesh)->axes[otherAxis(axis)];
    if (highCoordinate <= sourceAt(highAxis,highAxis.size()-1,157) + threshold) {
        const auto values = compat::ngRequireObject(mesh)->axes[axis];
        if (std::abs(component(mouse,otherAxis(axis),158) - sourceAt(values,0,158)) < threshold) {
            value = sourceAt(values,0,159); return true;
        }
        if (std::abs(component(mouse,otherAxis(axis),161) - sourceAt(values,values.size()-1,161)) < threshold) {
            value = sourceAt(values,values.size()-1,162); return true;
        }
    }
    return false;
}
void GridTool::setToolMode(VertexToolMode mode, EditorRef editor) {
    const auto keepAlive = shared_from_this();
#ifndef NDEBUG
    if (compat::ngRequireObject(editor)->deformOnly && mode == VertexToolMode::Grid)
        throw compat::ngStaticError<compat::AssertError>("Assertion failure", "source/nijigenerate/viewport/common/mesheditor/tools/grid.d", 172);
#endif
    isDragging = false; compat::ngRequireObject(editor)->isSelecting = false; compat::ngRequireObject(editor)->deselectAll(); hasPressOrigin = false;
}
bool GridTool::onDragStart(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    bool isVirtual = false;
    const auto mesh = getMesh(editor, isVirtual);
    if (!mesh) { currentAction = GridActionID::End; return false; }
    const auto start = hasPressOrigin ? pressVertex : compat::ngRequireObject(editor)->vtxAtMouse;
    auto retained = compat::ngRequireObject(editor)->getVerticesByIndex({start});
    auto* vertex = sourceAt(retained,0,197);
    if (vertex) {
        if (compat::ngRequireObject(mesh)->axes.size() != 2) { currentAction = GridActionID::End; return false; }
        currentAction = compat::ngRequireObject(mesh)->axes.size() == 2 ? GridActionID::TranslateFree : GridActionID::End;
        retained = compat::ngRequireObject(editor)->getVerticesByIndex({start}); vertex = sourceAt(retained,0,205);
        dragOrigin = compat::ngRequireObject(vertex)->position;
        const float threshold = selectRadius / viewport::base::viewportZoom;
        float x, y;
        const bool foundY = isOnEdge(mesh, 0, dragOrigin, threshold, y);
        const bool foundX = isOnEdge(mesh, 1, dragOrigin, threshold, x);
        if (foundY) {
            dragOrigin.y = y;
            if (!foundX) currentAction = GridActionID::TranslateX;
            else { dragTargetXIndex = find(compat::ngRequireObject(mesh)->axes[1], compat::ngRequireObject(vertex)->position.x); if (dragTargetXIndex < 0) currentAction = GridActionID::End; }
        }
        if (foundX) {
            dragOrigin.x = x;
            if (!foundY) currentAction = GridActionID::TranslateY;
            else { dragTargetYIndex = find(compat::ngRequireObject(mesh)->axes[0], compat::ngRequireObject(vertex)->position.y); if (dragTargetYIndex < 0) currentAction = GridActionID::End; }
        }
        if (!foundX) { dragTargetXIndex = find(compat::ngRequireObject(mesh)->axes[1], compat::ngRequireObject(vertex)->position.x); if (dragTargetXIndex < 0) currentAction = GridActionID::End; }
        if (!foundY) { dragTargetYIndex = find(compat::ngRequireObject(mesh)->axes[0], compat::ngRequireObject(vertex)->position.y); if (dragTargetYIndex < 0) currentAction = GridActionID::End; }
        return true;
    }
    if (compat::ngRequireObject(mesh)->axes.size() < 2 || compat::ngRequireObject(mesh)->vertices.empty()) {
        currentAction = GridActionID::Create; dragOrigin = hasPressOrigin ? pressOrigin : mouse; return true;
    }
    return false;
}
bool GridTool::onDragEnd(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    bool isVirtual = false;
    const auto mesh = getMesh(editor, isVirtual);
    if (!mesh) return false;
    if (currentAction == GridActionID::TranslateX || currentAction == GridActionID::TranslateY ||
        currentAction == GridActionID::TranslateFree) { currentAction = GridActionID::End; return true; }
    if (currentAction != GridActionID::Create) return false;
    dragEnd = mouse;
    const float left = std::min(dragOrigin.x, dragEnd.x), bottom = std::min(dragOrigin.y, dragEnd.y);
    const float width = std::max(dragOrigin.x, dragEnd.x) - left, height = std::max(dragOrigin.y, dragEnd.y) - bottom;
    MeshData data;
    allocateGridStorage([&] { data.gridAxes.resize(2); });
    const auto divisor = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(numCut) - 1u);
    for (std::int32_t i = 0; i < numCut; ++i) {
        const float y = bottom + height * i / divisor;
        allocateGridStorage([&] { data.gridAxes[0].push_back(y); });
        const float x = left + width * i / divisor;
        allocateGridStorage([&] { data.gridAxes[1].push_back(x); });
    }
    data.regenerateGrid(); compat::ngRequireObject(mesh)->copyFromMeshData(data);
    refreshMeshView(editor, mesh); markChanged(editor); currentAction = GridActionID::End; return true;
}
bool GridTool::onDragUpdate(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    bool isVirtual = false;
    const auto mesh = getMesh(editor, isVirtual);
    if (!mesh) return false;
    dragEnd = compat::ngRequireObject(editor)->mousePos;
    const bool y = currentAction == GridActionID::TranslateY || currentAction == GridActionID::TranslateFree;
    const bool x = currentAction == GridActionID::TranslateX || currentAction == GridActionID::TranslateFree;
    if (y) {
        const auto line=currentAction==GridActionID::TranslateFree?317u:306u;
        auto& axis=sourceAt(compat::ngRequireObject(mesh)->axes,0,line);
        sourceAt(axis,static_cast<std::size_t>(dragTargetYIndex),line)=mouse.y;
        sortAxis(axis); dragTargetYIndex=find(axis,mouse.y);
    }
    if (x) {
        const auto line=currentAction==GridActionID::TranslateFree?321u:295u;
        auto& axis=sourceAt(compat::ngRequireObject(mesh)->axes,1,line);
        sourceAt(axis,static_cast<std::size_t>(dragTargetXIndex),line)=mouse.x;
        sortAxis(axis); dragTargetXIndex=find(axis,mouse.x);
    }
    if (x || y) {
        MeshData data; data.gridAxes = compat::ngRequireObject(mesh)->axes; data.regenerateGrid(); compat::ngRequireObject(mesh)->copyFromMeshData(data);
        refreshMeshView(editor, mesh); markChanged(editor); return true;
    }
    return currentAction == GridActionID::Create;
}
bool GridTool::updateMeshEdit(ImGuiIO*, EditorRef editor, bool& changed) {
    const auto keepAlive = shared_from_this();
    changed = false;
    bool isVirtual = false;
    const auto mesh = getMesh(editor, isVirtual);
    if (!mesh) return false;
    constexpr auto button = ImGuiMouseButton_Left;
    if (isDragging && core::input::ngInputIsMouseReleased(button)) { onDragEnd(compat::ngRequireObject(editor)->mousePos, editor); isDragging = false; }
    if (ImGui::IsMouseClicked(button)) {
        compat::ngRequireObject(editor)->maybeSelectOne = UINT64_MAX; pressOrigin = compat::ngRequireObject(editor)->mousePos; pressVertex = compat::ngRequireObject(editor)->vtxAtMouse; hasPressOrigin = true;
    }
    const auto tip = []<std::size_t N, std::size_t K>(const char (&text)[N], const char (&key)[K]) {
        const auto actionText = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
        const auto keyText = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        widgets::ngStatusTooltip(actionText, keyText);
    };
    tip("Drag to define 2x2 mesh", "Left Mouse");
    tip("Add/remove key points to axes", "Left Mouse");
    tip("Change key point position in the axis", "Left Mouse");
    if (!isDragging && core::input::ngInputIsMouseReleased(button) && compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX)
        compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
    if (ImGui::IsMouseDoubleClicked(button)) {
        const auto retained = compat::ngRequireObject(editor)->getVerticesByIndex({compat::ngRequireObject(editor)->vtxAtMouse});
        const auto* vertex = sourceAt(retained,0,366);
        if (vertex) {
            const float x = compat::ngRequireObject(vertex)->position.x, y = compat::ngRequireObject(vertex)->position.y;
            if (compat::ngRequireObject(mesh)->axes.size() == 2) {
                const auto iy = findIndex(compat::ngRequireObject(mesh)->axes[0], y), ix = findIndex(compat::ngRequireObject(mesh)->axes[1], x);
                const bool edgeX = ix == 0 || static_cast<std::size_t>(ix) == compat::ngRequireObject(mesh)->axes[1].size() - 1;
                const bool edgeY = iy == 0 || static_cast<std::size_t>(iy) == compat::ngRequireObject(mesh)->axes[0].size() - 1;
                const auto remove = [](auto& axis, std::ptrdiff_t index) {
                    axis=axis.removed(static_cast<std::size_t>(index));
                };
                if (edgeX && edgeY) {}
                else if (edgeX) remove(compat::ngRequireObject(mesh)->axes[0], iy);
                else if (edgeY) remove(compat::ngRequireObject(mesh)->axes[1], ix);
                else { remove(compat::ngRequireObject(mesh)->axes[0], iy); remove(compat::ngRequireObject(mesh)->axes[1], ix); }
                MeshData data; data.gridAxes = compat::ngRequireObject(mesh)->axes; data.regenerateGrid(); compat::ngRequireObject(mesh)->copyFromMeshData(data);
                refreshMeshView(editor, mesh); markChanged(editor); compat::ngRequireObject(editor)->vtxAtMouse = UINT64_MAX;
            }
        } else if (compat::ngRequireObject(mesh)->axes.size() == 2) {
            const float threshold = selectRadius / viewport::base::viewportZoom;
            const auto mouse = compat::ngRequireObject(editor)->mousePos;
            float y, x;
            const bool foundY = isOnGrid(mesh, 0, mouse, threshold, y), foundX = isOnGrid(mesh, 1, mouse, threshold, x);
            const auto insert = [](auto& axis, float value) {
                for (std::size_t i = 0; i < axis.size(); ++i) if (value < axis[i]) {
                    allocateGridStorage([&] { axis.insertValue(i, value); });
                    break;
                }
            };
            if (!foundY) insert(compat::ngRequireObject(mesh)->axes[0], mouse.y);
            if (!foundX) insert(compat::ngRequireObject(mesh)->axes[1], mouse.x);
            MeshData data; data.gridAxes = compat::ngRequireObject(mesh)->axes; data.regenerateGrid(); compat::ngRequireObject(mesh)->copyFromMeshData(data);
            refreshMeshView(editor, mesh); markChanged(editor);
        }
    }
    if (!isDragging && viewport::base::ngDragStartedInViewport(button) && ImGui::IsMouseDown(button) &&
        core::input::ngInputIsDragRequested(button)) { onDragStart(compat::ngRequireObject(editor)->mousePos, editor); isDragging = true; }
    if (isDragging) onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor);
    return true;
}
void GridTool::draw(base::CameraRef, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    bool isVirtual = false;
    const auto mesh = getMesh(editor, isVirtual);
    if (!mesh) return;
    nicxlive::core::math::Vec3Array lines;
    const auto color = compat::ngRequireObject(editor)->edgeColor;
    if (currentAction == GridActionID::Create) {
        const float left = std::min(dragOrigin.x, dragEnd.x), bottom = std::min(dragOrigin.y, dragEnd.y);
        const float right = std::max(dragOrigin.x, dragEnd.x), top = std::max(dragOrigin.y, dragEnd.y);
        const auto divisor = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(numCut) - 1u);
        for (std::int32_t i = 0; i < numCut; ++i) {
            const float y = bottom + (top - bottom) * i / divisor, x = left + (right - left) * i / divisor;
            lines.append({left, y, 0}); lines.append({right, y, 0}); lines.append({x, bottom, 0}); lines.append({x, top, 0});
        }
    } else {
        if (compat::ngRequireObject(mesh)->axes.size() != 2) return;
        const float left=sourceAt(compat::ngRequireObject(mesh)->axes[1],0,475);
        const auto& rightAxis=compat::ngRequireObject(mesh)->axes[1];
        const float right=sourceAt(rightAxis,rightAxis.size()-1,476);
        const float bottom=sourceAt(compat::ngRequireObject(mesh)->axes[0],0,477);
        const auto& topAxis=compat::ngRequireObject(mesh)->axes[0];
        const float top=sourceAt(topAxis,topAxis.size()-1,478);
        const auto ys=compat::ngRequireObject(mesh)->axes[0];
        for (float y : ys) { lines.append({left, y, 0}); lines.append({right, y, 0}); }
        const auto xs=compat::ngRequireObject(mesh)->axes[1];
        for (float x : xs) { lines.append({x, bottom, 0}); lines.append({x, top, 0}); }
    }
    if (!lines.empty()) {
        core::dbg::inDbgSetBuffer(lines);
        core::dbg::inDbgDrawLines(color, nicxlive::core::math::Mat4::identity());
    }
}
}
