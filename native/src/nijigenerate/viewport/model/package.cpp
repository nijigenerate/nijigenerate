#include "package.hpp"
#include "deform.hpp"
#include "depthboneoverlay.hpp"
#include "onionslice.hpp"
#include "detail/phobos_stable_part_sort.hpp"
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/commands/depth/bone_effective_pivots.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nodes/part.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/driver.hpp>
#include <nodes/curve.hpp>
#include <runtime_state.hpp>
#include <param/parameter.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace nijigenerate::viewport::model {
using namespace nicxlive::core;
using namespace nicxlive::core::nodes;
using namespace nicxlive::core::math;
using namespace project;
using namespace widgets;
using namespace core::dbg;
using NodeRef = std::shared_ptr<Node>;
using Bone = std::shared_ptr<ext::nodes::ExDepthBone>;
using Root = std::shared_ptr<ext::nodes::ExDepthRigRoot>;
namespace {
thread_local compat::Slice<std::shared_ptr<Part>> foundParts;
constexpr int entrySize = 48;
template<class T> std::shared_ptr<T> allocateModelSubview() {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
}
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<std::size_t A, std::size_t B>
compat::RetainedString trChoice(bool condition, const char (&yes)[A], const char (&no)[B]) {
    return core::translation::ngTranslateRetained(condition ?
        compat::RetainedString::fromStaticLiteral(yes) : compat::RetainedString::fromStaticLiteral(no));
}
Vec2 sourceCurvePoint(const std::shared_ptr<Curve>& curve, float position) {
    // Source curves expose their genuine cached mutable evaluation; native
    // Curve implementations keep their existing virtual entry.
    if (auto* source = dynamic_cast<SourceCurveEvaluation*>(curve.get())) return source->pointSource(position);
    return compat::ngRequireObject(curve)->point(position);
}
std::array<float, 4> boundsOf(const std::shared_ptr<Drawable>& node) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    return node->bounds.value_or(std::array{nan, nan, nan, nan});
}
std::uint32_t floatOrder(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    return bits & 0x80000000u ? ~bits : bits | 0x80000000u;
}
}
thread_local ViewporMenuSortMode incViewportModelMenuSortMode = ViewporMenuSortMode::ZSort;
void ModelLayoutViewport::selectionChanged(compat::Slice<NodeRef> nodes) {
    depthboneoverlay::depthBoneEffectivePivotSelectionChanged(nodes);
}
void ModelLayoutViewport::withdraw() { depthboneoverlay::depthBoneEffectivePivotSelectionChanged({}); }
Root ModelLayoutViewport::findDepthRoot(Bone bone) {
    for (NodeRef cursor = bone; cursor; cursor = cursor->parentPtr())
        if (auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(cursor)) return root;
    return nullptr;
}
compat::Slice<Root> ModelLayoutViewport::findDepthRoots() {
    compat::Slice<Root> roots;
    const auto puppet = incActivePuppet();
    if (!puppet || !puppet->root) return roots;
    const auto visit = [&](auto&& self, NodeRef node) -> void {
        if (!node) return;
        if (auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node)) roots.append(root);
        const auto children = node->childrenList();
        for (auto child : children) self(self, child);
    };
    visit(visit, puppet->root);
    return roots;
}
void ModelLayoutViewport::draw(std::shared_ptr<Camera>) {
    Bone selectedBone;
    for (auto node : incSelectedNodes()) {
        if (auto bone = std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(node)) { selectedBone = bone; break; }
    }
    if (ngShowDepthBones) {
        for (auto root : findDepthRoots())
            depthboneoverlay::drawDepthBones(root, findDepthRoot(selectedBone) == root ? selectedBone : nullptr);
    }
    if (incSelectedNodes().empty()) return;
    const auto selectedNodes = incSelectedNodes();
    for (auto node : selectedNodes) {
        if (!node) continue;
        if (incShowOrientation && !std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node) &&
            !std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(node)) node->drawOrientation();
        if (incShowBounds) node->drawBounds();
        if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
            if (incShowVertices || incEditMode() != EditMode::ModelEdit) drawable->drawMeshLines();
        } else if (auto path = std::dynamic_pointer_cast<PathDeformer>(node)) {
            const auto drawLines = [](std::shared_ptr<Curve> curve, Mat4 matrix, Vec4 color) {
                if (!curve || curve->controlPoints().empty()) return;
                Vec3Array lines;
                for (int i = 1; i < 100; ++i) {
                    const auto a = sourceCurvePoint(curve, static_cast<float>((i - 1) / 100.0));
                    lines.push_back({a.x, a.y, 0});
                    const auto b = sourceCurvePoint(curve, static_cast<float>(i / 100.0));
                    lines.push_back({b.x, b.y, 0});
                }
                if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(color, matrix); }
            };
            const auto previousCurve = path->prevCurve;
            const auto previousTransform = path->transform().matrix();
            drawLines(previousCurve, previousTransform, {0.5f, 0.5f, 0.5f, 1});
            const auto deformedCurve = path->deformedCurve;
            const auto deformedTransform = path->transform().matrix();
            drawLines(deformedCurve, deformedTransform, {0.5f, 1, 0.5f, 1});
#if defined(NIJIGENERATE_DEBUG_PATH_DEFORM)
            const auto drawClosest = [](auto closest, Mat4 trans, Vec4 color, bool useDeformation) {
                if (!closest || closest->empty()) return;
                Vec3Array lines;
                for (auto [target, positions] : *closest) {
                    if (auto deform = std::dynamic_pointer_cast<Deformable>(target)) {
                        const auto inverse = core::math::source_matrix::inverse(trans);
                        const auto matrix = deform->transform().matrix();
                        const auto conv = Mat4::multiply(inverse, matrix);
                        // Initialization copies the descriptor. Vec2Array's
                        // assignment operator instead copies its element bytes.
                        const Vec2Array vertices = useDeformation ? Vec2Array{} : Vec2Array(deform->getVertices());
                        const auto count = useDeformation ? deform->getVertices().size() : vertices.size();
                        const auto deformCount = useDeformation ? deform->deformation.size() : 0;
                        for (std::size_t i = 0; i < count; ++i) {
                            auto v = useDeformation ? deform->getVertices().at(i) : vertices.at(i);
                            if (useDeformation && i < deformCount) {
                                auto d = deform->deformation.at(i); v.x += d.x; v.y += d.y;
                            }
                            const auto p = core::math::ngTransformHomogeneousPoint(conv, {v.x, v.y, 0});
                            lines.append({p.x, p.y, 0});
                            const auto q = positions.at(i);
                            lines.append({q.x, q.y, 0});
                        }
                    }
                }
                if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(color, trans); }
            };
            const auto originalClosest = path->closestPointsOriginal;
            const auto originalTransform = path->transform().matrix();
            drawClosest(originalClosest, originalTransform, {0.5f, 1, 1, 1}, false);
            const auto deformedClosest = path->closestPointsDeformed;
            const auto closestTransform = path->transform().matrix();
            drawClosest(deformedClosest, closestTransform, {0.5f, 1, 0.5f, 1}, true);
#endif
        } else if (auto grid = std::dynamic_pointer_cast<GridDeformer>(node)) {
            const auto baseVerts = grid->getVertices();
            if (baseVerts.size() >= 4) {
                const auto points = baseVerts.toArray();
                auto xs = detail::allocatePartSortMemory([&] { return std::vector<float>(points.size()); });
                for (std::size_t i = 0; i < points.size(); ++i) xs[i] = points[i].x;
                auto ys = detail::allocatePartSortMemory([&] { return std::vector<float>(points.size()); });
                for (std::size_t i = 0; i < points.size(); ++i) ys[i] = points[i].y;
                compat::phobos_float_sort::sort(xs); compat::phobos_float_sort::sort(ys);
                xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
                ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
                const auto cols = xs.size(), rows = ys.size();
                const bool haveDeform = grid->deformation.size() == baseVerts.size();
                if (cols >= 2 && rows >= 2 && cols * rows == baseVerts.size()) {
                    Vec3Array lines;
                    for (std::size_t y = 0; y < rows; ++y) for (std::size_t x = 0; x < cols; ++x) {
                        const auto index = y * cols + x;
                        auto start = points[index];
                        if (haveDeform) start = start + grid->deformation[index].toVector();
                        if (x + 1 < cols) {
                            auto right = points[index + 1];
                            if (haveDeform) right = right + grid->deformation[index + 1].toVector();
                            lines.push_back({start.x, start.y, 0}); lines.push_back({right.x, right.y, 0});
                        }
                        if (y + 1 < rows) {
                            auto down = points[index + cols];
                            if (haveDeform) down = down + grid->deformation[index + cols].toVector();
                            lines.push_back({start.x, start.y, 0}); lines.push_back({down.x, down.y, 0});
                        }
                    }
                    if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines({0.5f, 0.5f, 0.5f, 1}, grid->transform().matrix()); }
                }
            }
        }
        if (auto driver = std::dynamic_pointer_cast<Driver>(node)) driver->drawDebug();
    }
}
std::shared_ptr<base::Viewport> ModelViewport::createSubView(ModelEditSubMode mode) {
    switch (mode) {
    case ModelEditSubMode::Layout: return allocateModelSubview<ModelLayoutViewport>();
    case ModelEditSubMode::Deform: return allocateModelSubview<deform::DeformationViewport>();
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/model/package.d"), 242);
}
void ModelViewport::syncSubView() {
    const auto mode = ngModelEditSubMode();
    if (_subView && mode == activeSubMode) return;
    _subView = createSubView(mode);
    activeSubMode = mode;
    compat::ngRequireObject(_subView)->selectionChanged(incSelectedNodes());
    compat::ngRequireObject(_subView)->armedParameterChanged(incArmedParameter());
}
std::uint32_t ModelViewport::asyncDerivedUpdateViewportChannel() {
    return static_cast<std::uint32_t>(core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel::Model);
}
ModelViewport::ModelViewport() : activeSubMode(ModelEditSubMode::Layout) { _subView = createSubView(activeSubMode); }
void ModelViewport::draw(std::shared_ptr<Camera> camera) {
    syncSubView();
    compat::ngRequireObject(incActivePuppet())->update();
    commands::depth::bone::ngFlushDepthBoneEffectivePivotDirty();
    const auto onion = onionslice::OnionSlice::singleton();
    if (compat::ngRequireObject(onion)->enabled() && incArmedParameter()) {
        const auto parameter = incArmedParameter();
        const auto point = compat::ngRequireObject(parameter)->findClosestKeypoint();
        onion->capture({static_cast<std::uint32_t>(point.x), static_cast<std::uint32_t>(point.y)});
    }
    compat::ngRequireObject(incActivePuppet())->draw();
    onion->draw();
    DelegationViewport::draw(camera);
}
void ModelViewport::drawTools() { syncSubView(); DelegationViewport::drawTools(); }
void ModelViewport::drawOptions() {
    if (!incArmedParameter()) {
        if (ngBeginDropdownMenuRetained(compat::RetainedString::fromStaticLiteral("GIZMOS"),
            compat::RetainedString::fromStaticLiteral("\uefc9"))) {
            const ImVec4 disabled{0.6f, 0.6f, 0.6f, 1};
            if (ngButtonColored("\ue268", {0, 0}, incShowVertices ? colorUndefined : disabled)) incShowVertices = !incShowVertices;
            ngTooltip(trChoice(incShowVertices, "Hide Vertices", "Show Vertices"));
            ImGui::SameLine(0, 4);
            if (ngButtonColored("\ue3c6", {0, 0}, incShowBounds ? colorUndefined : disabled)) incShowBounds = !incShowBounds;
            ngTooltip(trChoice(incShowBounds, "Hide Bounds", "Show Bounds"));
            ImGui::SameLine(0, 4);
            if (ngButtonColored("\uefc9", {0, 0}, incShowOrientation ? colorUndefined : disabled)) incShowOrientation = !incShowOrientation;
            ngTooltip(trChoice(incShowOrientation, "Hide Orientation Gizmo", "Show Orientation Gizmo"));
            ImGui::SameLine(0, 4);
            if (ngButtonColored("\ue8ef", {0, 0}, ngShowDepthBones ? colorUndefined : disabled)) {
                ngShowDepthBones = !ngShowDepthBones;
                depthboneoverlay::depthBoneEffectivePivotSelectionChanged(incSelectedNodes());
            }
            ngTooltip(trChoice(ngShowDepthBones, "Hide Depth Bones", "Show Depth Bones"));
            ImGui::SameLine(0, 0); ngDummy({4, 0}); ImGui::SameLine(0, 0);
            const float maximum = std::numeric_limits<float>::max();
            if (ngBeginDropdownMenuRetained(compat::RetainedString::fromStaticLiteral("COLOR"),
                compat::RetainedString::fromStaticLiteral("\ue40a"), {128, 0}, {maximum, maximum})) {
                const float nan = std::numeric_limits<float>::quiet_NaN();
                float color[3]{nan, nan, nan}, alpha = 1;
                inGetClearColor(color[0], color[1], color[2], alpha);
                ImGui::ColorPicker3(core::translation::ngTranslateCString("COLOR").data(), color, ImGuiColorEditFlags_NoSidePreview |
                    ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoBorder);
                const auto space = ngAvailableSpace();
                inSetClearColor(color[0], color[1], color[2], alpha);
                ngDummy({0, 4});
                if (ngButtonColored(core::translation::ngTranslateCString("Reset").data(), {space.x, 0})) core::ngResetClearColor();
                ngEndDropdownMenu();
            }
            ngTooltip(tr("Background Color"));
            ngEndDropdownMenu();
        }
        ngTooltip(tr("Gizmos"));
    }
    DelegationViewport::drawOptions();
}
void ModelViewport::drawConfirmBar() {
    if (ngModelEditSubMode() != ModelEditSubMode::Layout) return;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
    if (auto node = std::dynamic_pointer_cast<Deformable>(incSelectedNode())) {
        const auto& io = ImGui::GetIO();
        const auto text = core::translation::ngTranslateCString(ngHasDragDrop("_PUPPETNTREE") ?
            (io.KeyCtrl ? "\ue14f Merge Mesh" : "\ue14f Copy Mesh") : "\uf097 Edit Mesh");
        if (ngButtonColored(text.data(), {0, 26})) vertex::incVertexEditStartEditing(node);
        if (ImGui::BeginDragDropTarget()) {
            ngTooltip(trChoice(io.KeyCtrl, "Merge Mesh Data", "Copy Mesh Data"));
            const auto* payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE");
            if (payload) {
                auto* source = *compat::ngRequireObject(static_cast<Node* const*>(payload->Data));
                if (dynamic_cast<Deformable*>(source)) {
                    const auto sourceNode = std::dynamic_pointer_cast<Deformable>(source->shared_from_this());
                    incSetEditMode(EditMode::VertexEdit);
                    incSelectNode(node);
                    vertex::incVertexEditSetTarget(node);
                    incFocusCamera(node, {0, 0});
                    if (io.KeyCtrl) vertex::incVertexEditMergeMeshDataToTarget(node, sourceNode);
                    else vertex::incVertexEditCopyMeshDataToTarget(node, sourceNode);
                }
            }
            ImGui::EndDragDropTarget();
        } else ngTooltip(tr("Edit Mesh"));
        if (auto grid = std::dynamic_pointer_cast<GridDeformer>(node)) {
            ImGui::SameLine(0, 0);
            if (ngButtonColored(core::translation::ngTranslateCString("\ue28b Edit Depth Map").data(), {0, 26})) {
                incSetEditMode(EditMode::DepthEdit, false);
                incSelectNode(grid);
                incFocusCamera(grid, {0, 0});
            }
            ngTooltip(tr("Edit Depth Map"));
        }
    }
    ImGui::PopStyleVar();
}
void ModelViewport::menu() {
    if (subView()) compat::ngRequireObject(subView())->menu();
    if (ImGui::MenuItem(core::translation::ngTranslateCString(
            incViewportModelMenuSortMode == ViewporMenuSortMode::ZSort ? "Z-Sort" : "Size-Sort").data()))
        incViewportModelMenuSortMode = incViewportModelMenuSortMode == ViewporMenuSortMode::ZSort ?
            ViewporMenuSortMode::SizeSort : ViewporMenuSortMode::ZSort;
    ImGui::Separator();
    const auto selectedNode = incSelectedNode();
    const auto root = compat::ngRequireObject(incActivePuppet())->root;
    if (!nicxlive::core::ngResourceObjectEquals(selectedNode, root))
        if (ImGui::MenuItem(core::translation::ngTranslateCString("Focus Selected").data())) incFocusCamera(incSelectedNode());
    if (ImGui::BeginChild("FOUND_PARTS", {256, 256}, false)) {
        if (!foundParts.empty()) {
            const auto available = ngAvailableSpace();
            const auto parts = foundParts;
            for (auto part : parts) {
                ImGui::PushID(std::bit_cast<std::int32_t>(compat::ngRequireObject(part)->getUUID()));
                const auto nameSize = ngMeasureString(compat::RetainedStringSlice(compat::ngRequireObject(part)->getName()));
                const auto cursor = ImGui::GetCursorPos();
                if (ImGui::Selectable("###PartSelectable", false, ImGuiSelectableFlags_None, {available.x, entrySize})) {
                    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) incAddSelectNode(part);
                    else incSelectNode(part);
                    ImGui::PopID(); ImGui::EndChild(); ImGui::CloseCurrentPopup(); return;
                }
                ImGui::SetItemAllowOverlap();
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ngSetNodeDragDropPayload(part, ImGuiCond_Always);
                    ngDragdropNodeList(part);
                    ImGui::EndDragDropSource();
                }
                ImGui::SetCursorPos({cursor.x + 2, cursor.y + 2});
                const auto textures = part->textures;
                if (textures.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, textures.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/model/package.d"), 469);
                ngTextureSlotUntitled("ICON", textures[0], {entrySize - 4, entrySize - 4}, 24, ImGuiWindowFlags_NoInputs);
                ImGui::SetCursorPos({cursor.x + entrySize + 4, cursor.y + entrySize / 2 - nameSize.y / 2});
                ngText(compat::RetainedStringSlice(compat::ngRequireObject(part)->getName()));
                ImGui::SetCursorPos({cursor.x, cursor.y + entrySize + 3});
                ImGui::PopID();
            }
        } else ngText(tr("No parts found"));
    }
    ImGui::EndChild();
}
void ModelViewport::menuOpening() {
    foundParts.resize(0);
    const auto mouse = core::input::ngInputGetMousePosition() * -1.0f;
    for (const auto& part : compat::ngRequireObject(incActivePuppet())->getAllParts()) {
        const auto bounds = boundsOf(part);
        const float width = bounds[2] - bounds[0], height = bounds[3] - bounds[1];
        if (mouse.x >= bounds[0] + width || mouse.x <= bounds[0] ||
            mouse.y >= bounds[1] + height || mouse.y <= bounds[1]) continue;
        bool selected = false;
        for (auto node : incSelectedNodes()) {
            const auto selectedId = compat::ngRequireObject(node)->getUUID();
            const auto candidateId = compat::ngRequireObject(part)->getUUID();
            if (selectedId == candidateId) { selected = true; break; }
        }
        if (!selected) foundParts.append(part);
    }
    if (incViewportModelMenuSortMode == ViewporMenuSortMode::ZSort) {
        detail::phobosStablePartSort(foundParts, [](std::shared_ptr<Part> a, std::shared_ptr<Part> b) {
            const auto left = a->zSortNoOffset();
            const auto right = b->zSortNoOffset();
            return floatOrder(left) < floatOrder(right);
        });
    } else if (incViewportModelMenuSortMode == ViewporMenuSortMode::SizeSort) {
        detail::phobosStablePartSort(foundParts, [](std::shared_ptr<Part> a, std::shared_ptr<Part> b) {
            const auto aa = boundsOf(a);
            const auto left = (aa[2] - aa[0]) * (aa[3] - aa[1]);
            const auto bb = boundsOf(b);
            const auto right = (bb[2] - bb[0]) * (bb[3] - bb[1]);
            return floatOrder(left) < floatOrder(right);
        });
    } else throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Unknown sort mode"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/model/package.d"), 518);
}
bool ModelViewport::hasMenu() { return true; }
void ModelViewport::armedParameterChanged(std::shared_ptr<param::Parameter> parameter) {
    syncSubView();
    if (_subView) compat::ngRequireObject(_subView)->armedParameterChanged(parameter);
}
}
