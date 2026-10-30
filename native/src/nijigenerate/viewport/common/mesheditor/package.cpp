// Independent source/nijigenerate/viewport/common/mesheditor/package.d port. BSD-2-Clause.
#include "package.hpp"
#include "tools/package.hpp"
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/mesheditor/tool_commands.hpp>
#include <nijigenerate/windows/flipconfig.hpp>
#include <nijigenerate/utils/transform.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor {
using namespace core::actionstack;
using nicxlive::core::nodes::NotifyReason;
namespace {
IncMeshEditor::Nodes editorKeys(const tools::base::Editors& source) {
    auto result = source.keys();
    if (result.isNull()) return {};
    return IncMeshEditor::Nodes::retainedView(result.retentionOwner(), result.data(), result.size());
}
compat::Slice<IncMeshEditor::EditorRef> editorValues(const tools::base::Editors& source) {
    auto result = source.values();
    if (result.isNull()) return {};
    return compat::Slice<IncMeshEditor::EditorRef>::retainedView(
        result.retentionOwner(), result.data(), result.size());
}
IncMeshEditor::EditorRef firstEditorValue(const tools::base::Editors& source, std::size_t line) {
    const auto values = editorValues(source);
    if (values.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), line);
    return values.data()[0];
}
IncMeshEditor::EditorRef readEditor(const tools::base::Editors& source,
        IncMeshEditor::NodeRef key, std::size_t line) {
    const auto table = source;
    if (!table.isNull()) {
        const auto found = table.find(key);
        if (found != table.end()) return found->second;
    }
    throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), line);
}
}
void IncMeshEditor::setTarget(NodeRef target) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (target) addTarget(target);
}
IncMeshEditor::EditorRef IncMeshEditor::getEditorFor(NodeRef drawing) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!editors.isNull() && editors.contains(drawing)) return readEditor(editors, drawing, 61);
    return {};
}
void IncMeshEditor::removeTarget(NodeRef target) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!editors.isNull() && editors.contains(target)) {
        const auto editor = readEditor(editors, target, 71);
        compat::ngRequireObject(editor)->abortToolMode();
        if (!editors.isNull()) editors.erase(target);
    }
}
IncMeshEditor::Nodes IncMeshEditor::getTargets() {
    return editorKeys(editors);
}
void IncMeshEditor::refreshMesh() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->refreshMesh(); }
}
void IncMeshEditor::abortActiveTools() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->abortToolMode(); }
}
void IncMeshEditor::releasePendingActions() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& [target, editor] : map) if (editor) compat::ngRequireObject(editor)->releasePendingActions();
}
void IncMeshEditor::resetMesh() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->resetMesh(); }
}
void IncMeshEditor::applyPreview() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->applyPreview(); }
}
void IncMeshEditor::applyToTarget() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    incActionPushGroup(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->applyToTarget(); }
    incActionPopGroup();
}
bool IncMeshEditor::update(ImGuiIO* io, CameraRef camera) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    bool result = false;
    incActionPushGroup();
    compat::Slice<std::int32_t> actions;
    { const auto map = editors; if (!map.isNull()) for (const auto& entry : map) {
        auto editor = entry.second; actions.append(compat::ngRequireObject(editor)->peek(io, camera));
    } }
    std::int32_t action = 0;
    if (!editorKeys(editors).empty()) {
        const auto keys = editorKeys(editors);
        if (keys.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, keys.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 121);
        action = compat::ngRequireObject(readEditor(editors, keys.data()[0], 121))->unify(actions);
    }
    { const auto map = editors; if (!map.isNull()) for (const auto& entry : map) {
        auto editor = entry.second; result = compat::ngRequireObject(editor)->update(io, camera, action) || result;
    } }
    { const auto map = editors; if (!map.isNull()) for (const auto& entry : map) {
        auto editor = entry.second;
        const auto target = compat::ngRequireObject(compat::ngRequireObject(editor)->getTarget());
        const auto argument = compat::ngRequireObject(editor)->getTarget();
        target->notifyChange(argument, NotifyReason::AttributeChanged);
    } }
    incActionPopGroup(); return result;
}
void IncMeshEditor::draw(CameraRef camera) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) { auto editor = entry.second; compat::ngRequireObject(editor)->draw(camera); }
}
void IncMeshEditor::setToolMode(VertexToolMode mode) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (auto info = tools::ngGetToolInfoOf(mode)) {
        const auto map = editors;
        if (!map.isNull()) for (const auto& entry : map) compat::ngRequireObject(info)->setupToolMode(entry.second, mode);
        toolMode = mode;
    }
}
void IncMeshEditor::viewportTools() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ImGui::SetWindowFontScale(1.30f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
    for (auto info : tools::incGetToolInfo()) {
        const auto currentInfo = compat::ngRequireObject(info);
        const bool currentDeformOnly = deformOnly;
        const auto currentMode = getToolMode();
        const auto currentEditors = editors;
        if (currentInfo->viewportTools(currentDeformOnly, currentMode, currentEditors)) {
            auto context = std::allocate_shared<commands::Context>(
                nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
            auto command = commands::mesheditor::tool::ngEnsureSelectToolModeCommand(compat::ngRequireObject(info)->mode());
            commands::ngRunCommand(command, context);
        }
    }
    ImGui::PopStyleVar(2); ImGui::SetWindowFontScale(1);
}
void IncMeshEditor::displayGroupIds() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (editorValues(editors).empty()) return;
    auto drawable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDrawable>(firstEditorValue(editors, 176));
    if (!drawable || editors.isNull() || editors.empty() ||
            compat::ngRequireObject(compat::ngRequireObject(drawable)->getMesh())->maxGroupId <= 1) return;
    auto editor = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDrawable>(firstEditorValue(editors, 178));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 10));
    const auto color = [](bool chosen) { return chosen ? widgets::colorUndefined : ImVec4(.6f,.6f,.6f,1); };
    if (widgets::ngButtonColored("All", ImVec2(0,0), color(compat::ngRequireObject(editor)->getGroupId() == 0))) {
        const auto map = editors; if (!map.isNull()) for (const auto& entry : map) compat::ngRequireObject(entry.second)->setGroupId(0);
    }
    ImGui::SameLine();
    const auto end = static_cast<std::uint32_t>(
        1u + compat::ngRequireObject(compat::ngRequireObject(editor)->getMesh())->maxGroupId);
    for (std::uint32_t index = 1; index < end; ++index) {
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(index)};
        auto formatted = core::translation::ngFormatTyped("%d", arguments);
        const auto label = [&] {
            try {
                return compat::RetainedString(std::move(formatted));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        const compat::SourceCString labelText(label);
        compat::detail::ngSourceCStringPostconditions(label.view(), labelText.data());
        const ImVec2 buttonSize(0, 0);
        const auto buttonColor = color(index == compat::ngRequireObject(editor)->getGroupId());
        if (widgets::ngButtonColored(labelText.data(), buttonSize, buttonColor)) {
            const auto map = editors; if (!map.isNull()) for (const auto& entry : map) compat::ngRequireObject(entry.second)->setGroupId(index);
        }
        ImGui::SameLine();
    }
    ImGui::PopStyleVar(2); ImGui::SetWindowFontScale(1);
}
void IncMeshEditor::displayToolOptions() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    for (auto info : tools::incGetToolInfo()) {
        const auto comparedMode = toolMode;
        if (comparedMode == compat::ngRequireObject(info)->mode()) {
            const auto currentInfo = compat::ngRequireObject(info);
            const bool currentDeformOnly = deformOnly;
            const auto currentMode = toolMode;
            const auto currentEditors = editors;
            currentInfo->displayToolOptions(currentDeformOnly, currentMode, currentEditors);
        }
    }
}
void IncMeshEditor::resetSelection() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto parameter = project::incArmedParameter();
    const auto point = compat::ngRequireObject(parameter)->findClosestKeypoint();
    actions::parameter::Bindings bindings;
    { const auto map = editors; if (!map.isNull()) for (const auto& entry : map)
        bindings.push_back(compat::ngRequireObject(parameter)->getOrAddBinding(
            entry.first, compat::RetainedString::fromStaticLiteral("deform"))); }
    auto action = actions::parameter::ngCreateParameterBindingsAction<
        actions::parameter::ParameterChangeBindingsValueAction>(
        compat::RetainedString::fromStaticLiteral("reset selection"), parameter, bindings,
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(point.x)),
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(point.y)));
    const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) {
        auto drawing = entry.first; auto editor = entry.second;
        auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
            compat::ngRequireObject(parameter)->getOrAddBinding(
                drawing, compat::RetainedString::fromStaticLiteral("deform")));
#ifndef NDEBUG
        if (!binding) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 224);
#endif
        auto values = compat::ngRequireObject(binding)->values;
        if (point.x >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(point.x, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 236);
        auto row = values.data()[point.x];
        if (point.y >= row.size()) throw compat::ngStaticError<compat::ArrayIndexError>(point.y, row.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 236);
        const auto valueOwner = row.retainPointer(point.y);
        auto& offsets = valueOwner->vertexOffsets;
        if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(drawing)) {
            offsets.resize(compat::ngRequireObject(drawable)->getVertices().size());
            const auto vertexEnd = compat::ngRequireObject(drawable)->getVertices().size();
            for (std::size_t i = 0; i < vertexEnd; ++i)
                for (auto selected : compat::ngRequireObject(editor)->selected) if (selected == i) {
                    if (i >= offsets.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, offsets.size(),
                        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 232);
                    offsets.set(i, {0,0});
                    break;
                }
        }
        auto setGrid = compat::ngRequireObject(binding)->getIsSet();
        if (point.x >= setGrid.size()) throw compat::ngStaticError<compat::ArrayIndexError>(point.x, setGrid.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 237);
        auto setRow = setGrid.data()[point.x];
        if (point.y >= setRow.size()) throw compat::ngStaticError<compat::ArrayIndexError>(point.y, setRow.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 237);
        setRow.data()[point.y] = true;
    }
    action->updateNewState(); incActionPush(action);
    model::deform::ngViewportNodeDeformNotifyParamValueChanged();
}
void IncMeshEditor::flipSelection() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto parameter = project::incArmedParameter();
    const auto point = compat::ngRequireObject(parameter)->findClosestKeypoint();
    actions::parameter::Bindings bindings;
    { const auto map = editors; if (!map.isNull()) for (const auto& entry : map)
        bindings.push_back(compat::ngRequireObject(parameter)->getOrAddBinding(
            entry.first, compat::RetainedString::fromStaticLiteral("deform"))); }
    incActionPushGroup();
    auto action = actions::parameter::ngCreateParameterBindingsAction<
        actions::parameter::ParameterChangeBindingsValueAction>(
        compat::RetainedString::fromStaticLiteral("Flip selection horizontaly from mirror"), parameter, bindings,
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(point.x)),
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(point.y)));
    const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) {
        auto editor = entry.second;
        auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
            compat::ngRequireObject(parameter)->getOrAddBinding(
                entry.first, compat::RetainedString::fromStaticLiteral("deform")));
#ifndef NDEBUG
        if (!binding) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/package.d"), 258);
#endif
        auto target = compat::ngRequireObject(binding)->getTarget().target.lock();
        if (target) {
            auto pair = windows::flipconfig::ngGetFlipPairFor(target);
            auto other = utils::transform::ngBindingGetPairFor(parameter, target, pair,
                compat::RetainedString(compat::ngRequireObject(binding)->getNameDescriptor()), false);
            utils::transform::ngBindingAutoFlip(binding, other,
                {static_cast<std::uint32_t>(point.x), static_cast<std::uint32_t>(point.y)}, 0, true, &compat::ngRequireObject(editor)->selected);
        }
    }
    action->updateNewState(); incActionPush(action); incActionPopGroup();
    model::deform::ngViewportNodeDeformNotifyParamValueChanged();
}
void IncMeshEditor::popupMenu() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    bool any = false; const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) if (!compat::ngRequireObject(entry.second)->selected.empty()) { any = true; break; }
    if (!any) return;
    const auto resetLabel = core::translation::ngTranslateCString("Reset selected");
    if (ImGui::MenuItem(resetLabel.data(), "", false, true)) resetSelection();
    const auto flipLabel = core::translation::ngTranslateCString("Flip selected from mirror");
    if (ImGui::MenuItem(flipLabel.data(), "", false, true)) flipSelection();
}
void IncMeshEditor::setMirrorHoriz(bool value) {
    mirrorHoriz = value; const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) {
        if (!entry.second) compat::ngFailNullAccess(true);
        entry.second->mirrorHoriz = value;
    }
}
void IncMeshEditor::setMirrorVert(bool value) {
    mirrorVert = value; const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) {
        if (!entry.second) compat::ngFailNullAccess(true);
        entry.second->mirrorVert = value;
    }
}
void IncMeshEditor::setPreviewTriangulate(bool value) {
    // Preserve the source's self-assignment of mirrorVert, not previewTriangulate.
    mirrorVert = mirrorVert; const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) {
        if (!entry.second) compat::ngFailNullAccess(true);
        entry.second->previewTriangulate = value;
    }
}
bool IncMeshEditor::previewingTriangulation() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock(); const auto map = editors;
    if (!map.isNull()) for (const auto& entry : map) if (!compat::ngRequireObject(entry.second)->previewingTriangulation()) return false;
    return true;
}
}
