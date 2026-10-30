#include "package.hpp"
#include "mesheditor/editor.hpp"
#include "automesh/optimum.hpp"
#include "automesh/contours.hpp"
#include "automesh/grid.hpp"
#include "automesh/skeletonize.hpp"
#include "detail/phobos_stable_node_sort.hpp"
#include <nijigenerate/viewport/main_viewport.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/core/asyncderivedupdate.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/viewport/depth/detail/ieee_float_order.hpp>
#include <nijigenerate/actions/vertex.hpp>
#include <nijigenerate/actions/deformable.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <render/immediate.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/part.hpp>
#include <nodes/composite.hpp>
#include <nodes/grid_deformer.hpp>
#include <algorithm>
#include <functional>
#include <tuple>
#include <nijigenerate/platform/automesh_batch_stack.hpp>

namespace nijigenerate::viewport::vertex {
using namespace nicxlive::core::nodes;
using namespace nicxlive::core::math;
using namespace project;
using namespace widgets;
using namespace core::actionstack;
using core::translation::ngTranslateCString;
using core::translation::ngTranslateRetained;
using common::mesheditor::IncMeshEditor;
using common::mesheditor::operations::impl::IncMeshEditorOneDrawable;
using common::mesheditor::operations::impl::IncMeshEditorOneDeformable;
using mesheditor::editor::VertexMeshEditor;
using Processor = std::shared_ptr<automesh::AutoMeshProcessor>;
using NodeRef = std::shared_ptr<Node>;
namespace {
thread_local compat::Slice<Processor> autoMeshProcessors{
    std::allocate_shared<automesh::optimum::OptimumAutoMeshProcessor>(
        nicxlive::core::d_runtime::SourceAllocator<automesh::optimum::OptimumAutoMeshProcessor>{}),
    std::allocate_shared<automesh::contours::ContourAutoMeshProcessor>(
        nicxlive::core::d_runtime::SourceAllocator<automesh::contours::ContourAutoMeshProcessor>{}),
    std::allocate_shared<automesh::grid::GridAutoMeshProcessor>(
        nicxlive::core::d_runtime::SourceAllocator<automesh::grid::GridAutoMeshProcessor>{}),
    std::allocate_shared<automesh::skeletonize::SkeletonExtractor>(
        nicxlive::core::d_runtime::SourceAllocator<automesh::skeletonize::SkeletonExtractor>{})};
thread_local Processor activeProcessor;
thread_local bool isSubPartsMeshVisible = false;

template<class Range> decltype(auto) sourceElement(Range& values, std::size_t index,
        std::size_t sourceLine) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/package.d"), sourceLine);
    return values[index];
}
void runAutoMesh(const std::shared_ptr<IncMeshEditor>& editor, NodeRef node) {
    const std::size_t stackBytes = platform::ngAutoMeshBatchFiberStackBytes();
    if (auto drawableEditor = std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(
            compat::ngRequireObject(editor)->getEditorFor(node))) {
        auto drawable = std::dynamic_pointer_cast<Drawable>(node);
        if (!drawable) return;
        core::tasks::TaskFiber fiber([drawable, drawableEditor] {
            const auto processor = compat::ngRequireObject(ngActiveAutoMeshProcessor());
            const auto currentMesh = drawableEditor->getMesh();
            const auto mirrorHoriz = drawableEditor->mirrorHoriz;
            const auto mirrorVert = drawableEditor->mirrorVert;
            auto mesh = processor->autoMesh(drawable, currentMesh, mirrorHoriz, 0, mirrorVert, 0);
            drawableEditor->setMesh(mesh);
        }, stackBytes);
        while (fiber.state() != core::tasks::TaskFiber::State::term) fiber.call();
    } else if (auto deformEditor = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(
            compat::ngRequireObject(editor)->getEditorFor(node))) {
        auto deform = std::dynamic_pointer_cast<Deformable>(node);
        if (!deform) return;
        auto mesh = common::mesh::ngCreateIncMesh(deform->getVertices());
        core::tasks::TaskFiber fiber([deform, deformEditor, &mesh] {
            mesh = compat::ngRequireObject(ngActiveAutoMeshProcessor())->autoMesh(deform, mesh,
                deformEditor->mirrorHoriz, 0, deformEditor->mirrorVert, 0);
        }, stackBytes);
        while (fiber.state() != core::tasks::TaskFiber::State::term) fiber.call();
        auto positions = common::mesh::ngMeshPositions(mesh);
        deformEditor->vertices = common::mesh::ngMeshVerticesFromPositions(positions);
        deformEditor->vertexMapDirty = true;
        deformEditor->refreshMesh();
    }
}
std::shared_ptr<IncMeshEditor> createCommandVertexEditor(NodeRef target) {
    auto editor = VertexMeshEditor::createSource();
    editor->setTargets(IncMeshEditor::Nodes{target});
    editor->resetMesh();
    return editor;
}
}
std::uint32_t VertexViewport::asyncDerivedUpdateViewportChannel() {
    return static_cast<std::uint32_t>(core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel::Model);
}
void VertexViewport::draw(std::shared_ptr<Camera> camera) {
    const auto keepAlive = weak_from_this().lock();
    const auto targets = compat::ngRequireObject(editor)->getTargets();
    for (auto target : targets) {
        if (auto part = std::dynamic_pointer_cast<Part>(target)) {
            const auto originalDeformation = part->deformation.dup();
            compat::ngRunWithScopeExit([&] {
                if (part->deformation.size() != part->getVertices().size()) part->deformation.resize(part->getVertices().size());
                for (std::size_t i = 0; i < part->deformation.size(); ++i) part->deformation.set(i, {0, 0});
                part->refreshDeform();
                auto texture = part->textures.empty() ? nullptr : part->textures[0];
                if (texture) {
                    const auto origin = part->Drawable::getMesh().origin;
                    nicxlive::core::render::inDrawTextureAtPosition(*texture, {-origin.x, -origin.y},
                        part->opacity, part->tint, part->screenTint);
                } else {
                    auto inverse = std::make_shared<Mat4>(ext::nodes::excamera_detail::inverse(part->transform().matrix()));
                    part->setOneTimeTransform(inverse);
                    part->drawOne();
                    part->setOneTimeTransform(nullptr);
                }
            }, [&] {
                part->deformation = originalDeformation;
                part->refreshDeform();
            });
        } else if (compat::ngRequireObject(target)->coverOthers()) {
            auto inverse = std::make_shared<Mat4>(ext::nodes::excamera_detail::inverse(target->transform().matrix()));
            target->setOneTimeTransform(inverse);
            compat::Slice<NodeRef> subParts;
            std::function<void(NodeRef)> find = [&](NodeRef node) {
                if (compat::ngRequireObject(node)->coverOthers()) {
                    auto children = node->children;
                    for (auto child : children) find(child);
                }
                if (auto composite = std::dynamic_pointer_cast<Composite>(node)) {
                    if (composite->propagateMeshGroup) subParts.append(composite);
                } else if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
                    subParts.append(drawable);
                    auto children = node->children;
                    for (auto child : children) find(child);
                }
            };
            find(target);
            detail::phobosStableNodeSort(subParts, [](auto a, auto b) {
                const auto left = compat::ngRequireObject(a)->zSort();
                const auto right = compat::ngRequireObject(b)->zSort();
                return depth::detail::ieeeFloatOrderKey(left) > depth::detail::ieeeFloatOrderKey(right);
            });
            for (auto part : subParts) compat::ngRequireObject(part)->drawOne();
            if (isSubPartsMeshVisible) for (auto node : subParts) {
                if (!nicxlive::core::ngResourceObjectEquals(target, node))
                    if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) drawable->drawMeshLines();
            }
            target->setOneTimeTransform(nullptr);
        }
    }
    compat::ngRequireObject(editor)->draw(camera);
}
void VertexViewport::drawTools() {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(editor)->viewportTools();
}
void VertexViewport::drawOptions() {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(editor)->displayGroupIds();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    const ImVec4 gray{0.6f, 0.6f, 0.6f, 1};
    ImGui::BeginGroup();
    if (ngButtonColored("\uea0f", {0, 0}, isSubPartsMeshVisible ? colorUndefined : gray))
        isSubPartsMeshVisible = !isSubPartsMeshVisible;
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Toggle mesh visibility")));
    ImGui::EndGroup();
    ImGui::SameLine(0, 4);
    ImGui::BeginGroup();
    const auto flip = [this](bool horizontal) {
        auto selected = incSelectedNodes();
        for (auto node : selected) {
            if (auto targetEditor = std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(
                    compat::ngRequireObject(editor)->getEditorFor(node))) {
                if (horizontal) compat::ngRequireObject(targetEditor->getMesh())->flipHorz();
                else compat::ngRequireObject(targetEditor->getMesh())->flipVert();
            } else if (auto targetEditor = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(
                    compat::ngRequireObject(editor)->getEditorFor(node))) {
                auto action = actions::vertex::VertexMoveAction::create(actions::ActionText(
                    horizontal ? compat::RetainedString::fromStaticLiteral("flip vertices horizontally") :
                        compat::RetainedString::fromStaticLiteral("flip vertices vertically")), targetEditor);
                const auto vertices = targetEditor->vertices;
                for (auto* vertex : vertices) {
                    const auto currentVertex = compat::ngRequireObject(vertex);
                    action->moveVertex(vertex, horizontal ?
                        Vec2{-currentVertex->position.x, currentVertex->position.y} :
                        Vec2{currentVertex->position.x, -currentVertex->position.y});
                }
                incActionPush(action);
            }
        }
    };
    if (ngButtonColored("\uea18")) flip(true);
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Flip Horizontally")));
    ImGui::SameLine(0, 0);
    if (ngButtonColored("\ue8d5")) flip(false);
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Flip Vertically")));
    ImGui::EndGroup();
    ImGui::SameLine(0, 4);
    ImGui::BeginGroup();
    if (ngButtonColored("\ue00f", {0, 0}, compat::ngRequireObject(editor)->getMirrorHoriz() ? colorUndefined : gray)) {
        compat::ngRequireObject(editor)->setMirrorHoriz(!compat::ngRequireObject(editor)->getMirrorHoriz());
        compat::ngRequireObject(editor)->refreshMesh();
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Mirror Horizontally")));
    ImGui::SameLine(0, 0);
    if (ngButtonColored("\ue011", {0, 0}, compat::ngRequireObject(editor)->getMirrorVert() ? colorUndefined : gray)) {
        compat::ngRequireObject(editor)->setMirrorVert(!compat::ngRequireObject(editor)->getMirrorVert());
        compat::ngRequireObject(editor)->refreshMesh();
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Mirror Vertically")));
    ImGui::EndGroup();
    ImGui::SameLine(0, 4);
    ImGui::BeginGroup();
    if (ngButtonColored("\ue3c8", {0, 0}, compat::ngRequireObject(editor)->getPreviewTriangulate()
            ? ImVec4(1, 1, 0, 1) : colorUndefined)) {
        compat::ngRequireObject(editor)->setPreviewTriangulate(
            !compat::ngRequireObject(editor)->getPreviewTriangulate());
        compat::ngRequireObject(editor)->refreshMesh();
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Triangulate vertices")));
    if (ngBeginDropdownMenuRetained(compat::RetainedString::fromStaticLiteral("TRIANGULATE_SETTINGS"))) {
        ngDummyLabel("TODO: Options Here", {0, 192});
        const auto bakeLabel = ngTranslateCString("Bake");
        if (ngButtonColored(bakeLabel.data(), {ngAvailableSpace().x, 0},
            compat::ngRequireObject(editor)->previewingTriangulation() ? colorUndefined : gray)) {
            if (compat::ngRequireObject(editor)->previewingTriangulation()) {
                compat::ngRequireObject(editor)->applyPreview();
                compat::ngRequireObject(editor)->refreshMesh();
            }
        }
        ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Bakes the triangulation, applying it to the mesh.")));
        ngEndDropdownMenu();
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Triangulation Options")));
    ImGui::EndGroup();
    ImGui::SameLine(0, 4);
    ImGui::BeginGroup();
    if (ngButtonColored("\ue886")) {
        for (auto node : compat::ngRequireObject(editor)->getTargets()) runAutoMesh(editor, node);
        compat::ngRequireObject(editor)->refreshMesh();
    }
    if (ngBeginDropdownMenuRetained(compat::RetainedString::fromStaticLiteral("AUTOMESH_SETTINGS"))) {
        ImGui::BeginGroup();
        auto processors = autoMeshProcessors;
        for (auto processor : processors) {
            const auto iconText = compat::ngRequireObject(processor)->icon();
            const compat::SourceCString icon{compat::RetainedStringSlice(iconText)};
            compat::detail::ngSourceCStringPostconditions(iconText.view(), icon.data());
            if (ngButtonColored(icon.data(), {0, 0},
                    automesh::ngAutoMeshProcessorObjectEquals(processor, ngActiveAutoMeshProcessor())
                        ? colorUndefined : gray))
                ngActiveAutoMeshProcessor(processor);
            ImGui::SameLine(0, 2);
        }
        ImGui::EndGroup();
        compat::ngRequireObject(ngActiveAutoMeshProcessor())->configure();
        const auto bakeLabel = ngTranslateCString("Bake");
        if (ngButtonColored(bakeLabel.data(), {ngAvailableSpace().x, 0})) {
            for (auto node : compat::ngRequireObject(editor)->getTargets()) runAutoMesh(editor, node);
            compat::ngRequireObject(editor)->refreshMesh();
        }
        ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Bakes the auto mesh.")));
        ngEndDropdownMenu();
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Auto Meshing Options")));
    ImGui::EndGroup();
    ImGui::SameLine(0, 4);
    compat::ngRequireObject(editor)->displayToolOptions();
    ImGui::PopStyleVar(2);
}
void VertexViewport::drawConfirmBar() {
    const auto keepAlive = weak_from_this().lock();
    using namespace widgets::dialog;
    const auto targets = compat::ngRequireObject(editor)->getTargets();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 4));
    const auto applyLabel = ngTranslateCString("\ue668 Apply");
    if (ngButtonColored(applyLabel.data(), {0, 26})) {
        if (incMeshEditGetIsApplySafe()) incMeshEditApply();
        else {
            const auto title = ngTranslateCString("Are you sure?");
            const auto body = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "The layout of the mesh has changed, all deformations to this mesh will be deleted if you continue."));
            const auto bodyOwner = body.retentionOwner();
            ngDialog(DialogTag::borrowed("CONFIRM_VERTEX_APPLY"), DialogTag::retained(title.data(), title.owner),
                bodyOwner.use_count() ? DialogText::retained(body.view(), bodyOwner) : DialogText::borrowed(body.view()),
                DialogLevel::Warning, DialogButtons::Yes | DialogButtons::No);
        }
    }
    if (ngDialogButtonSelected("CONFIRM_VERTEX_APPLY") == DialogButtons::Yes) incMeshEditApply();
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Apply")));
    ImGui::SameLine(0, 0);
    const auto cancelLabel = ngTranslateCString("\ue5cd Cancel");
    if (ngButtonColored(cancelLabel.data(), {0, 26})) {
        if (ImGui::GetIO().KeyShift) incMeshEditReset(); else incMeshEditClear();
        if (auto vertexEditor = std::dynamic_pointer_cast<VertexMeshEditor>(editor)) vertexEditor->closeScope();
        incSetEditMode(EditMode::ModelEdit);
        for (auto target : targets) incAddSelectNode(target);
        incFocusCamera(sourceElement(targets, 0, 345));
    }
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Cancel")));
    ImGui::PopStyleVar();
}
void VertexViewport::update(ImGuiIO* io, std::shared_ptr<Camera> camera) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(editor)->update(io, camera);
}
void VertexViewport::withdraw() {
    const auto keepAlive = weak_from_this().lock();
    if (auto vertexEditor = std::dynamic_pointer_cast<VertexMeshEditor>(editor)) vertexEditor->closeScope();
    editor.reset();
}
void VertexViewport::present() {
    const auto keepAlive = weak_from_this().lock();
    editor = VertexMeshEditor::createSource();
}
void VertexViewport::copyMeshDataToTarget(DeformableRef target, DeformableRef source) {
    const auto keepAlive = weak_from_this().lock();
    if (!compat::ngRequireObject(editor)->getEditorFor(target)) {
        compat::ngRequireObject(editor)->addTarget(target);
#ifndef NDEBUG
        if (!compat::ngRequireObject(editor)->getEditorFor(target)) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/package.d"), 371);
#endif
    }
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(source)) {
        const auto receiver = compat::ngRequireObject(compat::ngRequireObject(editor)->getEditorFor(target));
        receiver->importMesh(drawable->Drawable::getMesh());
    } else {
        MeshData mesh;
        mesh.vertices = compat::ngRequireObject(source)->getVertices();
        mesh.uvs = compat::ngRequireObject(source)->getVertices();
        const auto receiver = compat::ngRequireObject(compat::ngRequireObject(editor)->getEditorFor(target));
        receiver->importMesh(mesh);
    }
}
void VertexViewport::mergeMeshDataToTarget(DeformableRef target, DeformableRef source) {
    const auto keepAlive = weak_from_this().lock();
    const auto sourceMatrix = compat::ngRequireObject(source)->transform().matrix();
    const auto targetMatrix = compat::ngRequireObject(target)->transform().matrix();
    const auto matrix = Mat4::multiply(sourceMatrix, ext::nodes::excamera_detail::inverse(targetMatrix));
    if (!compat::ngRequireObject(editor)->getEditorFor(target)) {
        compat::ngRequireObject(editor)->addTarget(target);
#ifndef NDEBUG
        if (!compat::ngRequireObject(editor)->getEditorFor(target)) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/package.d"), 388);
#endif
    }
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(source)) {
        const auto receiver = compat::ngRequireObject(compat::ngRequireObject(editor)->getEditorFor(target));
        receiver->mergeMesh(drawable->Drawable::getMesh(), matrix);
    } else {
        MeshData mesh;
        mesh.vertices = compat::ngRequireObject(source)->getVertices();
        mesh.uvs = compat::ngRequireObject(source)->getVertices();
        const auto receiver = compat::ngRequireObject(compat::ngRequireObject(editor)->getEditorFor(target));
        receiver->mergeMesh(mesh, matrix);
    }
}
void VertexViewport::apply() {
    const auto keepAlive = weak_from_this().lock();
    const auto targets = compat::ngRequireObject(editor)->getTargets();
    if (compat::ngRequireObject(editor)->previewingTriangulation()) {
        compat::ngRequireObject(editor)->applyPreview();
        compat::ngRequireObject(editor)->refreshMesh();
    }
    for (auto node : targets) if (auto drawable = std::dynamic_pointer_cast<Drawable>(node))
        (void)std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(compat::ngRequireObject(editor)->getEditorFor(drawable));
    if (auto vertexEditor = std::dynamic_pointer_cast<VertexMeshEditor>(editor)) vertexEditor->closeScope();
    compat::ngRequireObject(editor)->applyToTarget();
    incSetEditMode(EditMode::ModelEdit);
    for (auto node : targets) if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) incAddSelectNode(drawable);
    incFocusCamera(sourceElement(targets, 0, 433));
}
void VertexViewport::clear() {
    const auto keepAlive = weak_from_this().lock();
    for (auto node : compat::ngRequireObject(editor)->getTargets()) {
        if (auto meshEditor = std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(
                compat::ngRequireObject(editor)->getEditorFor(node))) {
            compat::ngRequireObject(meshEditor->getMesh())->clear();
        } else if (auto deformEditor = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(
                compat::ngRequireObject(editor)->getEditorFor(node))) {
            deformEditor->vertices.resize(0); deformEditor->vertexMapDirty = true; deformEditor->refreshMesh();
        }
    }
}
void VertexViewport::reset() {
    const auto keepAlive = weak_from_this().lock();
    for (auto node : compat::ngRequireObject(editor)->getTargets()) {
        if (auto meshEditor = std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(
                compat::ngRequireObject(editor)->getEditorFor(node))) {
            compat::ngRequireObject(meshEditor->getMesh())->reset();
        } else if (auto deformEditor = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(
                compat::ngRequireObject(editor)->getEditorFor(node))) deformEditor->resetMesh();
    }
}
std::shared_ptr<VertexViewport> incVertexViewport() {
    const auto viewport = compat::ngRequireObject(base::ngViewport());
    return std::dynamic_pointer_cast<VertexViewport>(viewport->subView());
}
void incVertexEditStartEditing(DeformableRef target) {
    incSetEditMode(EditMode::VertexEdit); incSelectNode(target); incVertexEditSetTarget(target); incFocusCamera(target, {0, 0});
}
void incVertexEditSetTarget(DeformableRef target) {
    if (auto view = incVertexViewport()) {
        const auto receiver = compat::ngRequireObject(view->editor);
        receiver->setTarget(target);
    }
}
void incVertexEditCopyMeshDataToTarget(DeformableRef target, DeformableRef source) {
    if (auto view = incVertexViewport()) view->copyMeshDataToTarget(target, source);
}
void incVertexEditMergeMeshDataToTarget(DeformableRef target, DeformableRef source) {
    if (auto view = incVertexViewport()) view->mergeMeshDataToTarget(target, source);
}
bool incMeshEditGetIsApplySafe() { return true; }
void incMeshEditApply() { if (auto view = incVertexViewport()) view->apply(); }
void incMeshEditClear() { if (auto view = incVertexViewport()) view->clear(); }
void incMeshEditReset() { if (auto view = incVertexViewport()) view->reset(); }
Processor ngActiveAutoMeshProcessor() {
    if (!activeProcessor) activeProcessor = sourceElement(autoMeshProcessors, 0, 529);
    return activeProcessor;
}
void ngActiveAutoMeshProcessor(Processor processor) { activeProcessor = processor; }
compat::Slice<Processor> ngAutoMeshProcessors() { return autoMeshProcessors; }
void ngInitializeVertexViewportModule() {
    // ODR-use the real TLS initializer at the import catalogue's thread entry.
    // Its language guard preserves the existing processor identities on repeat calls.
    (void)ngAutoMeshProcessors();
}
std::shared_ptr<IncMeshEditor> ngVertexViewportGetEditor() { if (auto view = incVertexViewport()) return view->editor; return {}; }
bool ngApplyDrawableMeshFromCommand(std::shared_ptr<Drawable> target,
    std::shared_ptr<common::mesh::IncMesh> mesh, compat::RetainedString& message) {
    message = {};
    if (!target) { message = compat::RetainedString::fromStaticLiteral("No drawable target"); return false; }
    if (!mesh) { message = compat::RetainedString::fromStaticLiteral("No mesh data"); return false; }
    auto editor = ngVertexViewportGetEditor();
    auto targetEditor = editor ? std::dynamic_pointer_cast<IncMeshEditorOneDrawable>(editor->getEditorFor(target)) : nullptr;
    if (!targetEditor) core::math::mesh::applyMeshToTarget(target, mesh->vertices, &mesh);
    else { targetEditor->setMesh(mesh); targetEditor->refreshMesh(); targetEditor->applyToTarget(); }
    target->notifyChange(target, NotifyReason::StructureChanged);
    return true;
}
bool ngApplyDeformableVerticesFromCommand(DeformableRef target, Vec2Array positions, compat::RetainedString& message) {
    message = {};
    if (!target) { message = compat::RetainedString::fromStaticLiteral("No deformable target"); return false; }
    if (positions.empty()) { message = compat::RetainedString::fromStaticLiteral("No vertex data"); return false; }
    if (auto grid = std::dynamic_pointer_cast<GridDeformer>(target)) {
        auto mesh = common::mesh::ngCreateIncMesh(positions);
        auto action = actions::deformable::GridDeformerDefineAction::createSource([&] {
            return std::tuple{actions::ActionText(ext::nodes::ngRetainNodeName(grid->getName())), grid};
        });
        core::math::mesh::applyMeshToTargetNoRecord(grid, mesh->vertices, &mesh);
        action->updateNewState(); incActionPush(action);
        ngRefreshDeformableCommandEditors(target); return true;
    }
    auto editor = ngVertexViewportGetEditor();
    auto targetEditor = editor ? std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor->getEditorFor(target)) : nullptr;
    if (targetEditor) {
        targetEditor->abortToolMode(); targetEditor->forceResetAction();
        targetEditor->vertices = common::mesh::ngMeshVerticesFromPositions(positions);
        targetEditor->refreshMesh(); targetEditor->applyToTarget(); targetEditor->vertexMapDirty = false;
        target->notifyChange(target, NotifyReason::StructureChanged); return true;
    }
    core::math::mesh::applyMeshToTarget(target, positions.toArray(), static_cast<std::shared_ptr<common::mesh::IncMesh>*>(nullptr));
    ngRefreshDeformableCommandEditors(target); return true;
}
void ngRefreshDeformableCommandEditors(DeformableRef target) {
    const auto refresh = [target](std::shared_ptr<IncMeshEditor> editor) {
        if (!editor) return;
        auto targetEditor = editor->getEditorFor(target);
        if (!targetEditor) return;
        targetEditor->abortToolMode(); targetEditor->forceResetAction(); targetEditor->resetMesh(); targetEditor->refreshMesh();
    };
    refresh(ngVertexViewportGetEditor());
    refresh(model::deform::ngViewportModelDeformGetEditor());
}
}
