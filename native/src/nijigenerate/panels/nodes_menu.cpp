#include "nodes_internal.hpp"
#include <nijigenerate/commands/node/dynamic.hpp>
#include <nijigenerate/commands/node/base.hpp>
#include <nijigenerate/commands/node/conversion_map.hpp>
#include <nijigenerate/commands/node/clipboard_state.hpp>
#include <nijigenerate/commands/parameter/paramedit.hpp>
#include <nijigenerate/commands/parameter/paramedit_table.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <nodes/deformable.hpp>
#include <imgui.h>

namespace nijigenerate::panels::nodes {
using namespace detail;
using namespace widgets;
void ngAddOrInsertNodeMenu(bool add) {
    ngText(tr("Suffix")); ImGui::SameLine();
    if (input("###NAME_SUFFIX", suffixName, ngAvailableSpace().x - 24)) {
        // The source retains the buffer when its UTF conversion fails.
        try {
            auto text = cstring(suffixName.view());
            suffixName = text.retainedString();
        } catch (const compat::UTFException&) {}
    }
    const auto create = [&]<std::size_t N>(const char (&name)[N], compat::RetainedString label) {
        const auto className = compat::RetainedString::fromStaticLiteral(name);
        ngText(utils::incTypeIdToIcon(className.view())); ImGui::SameLine(0, 2);
        if (ImGui::MenuItem(cstring(label.view()).data(), nullptr, false, true)) {
            auto ctx = std::allocate_shared<commands::Context>(
                nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
            ctx->puppet(project::incActivePuppet());
            auto selected = project::incSelectedNodes();
            if (!selected.empty()) ctx->nodes(selected);
            std::shared_ptr<commands::Command> command;
            if (add) command = commands::node::dynamic::ensureAddNodeCommand(className, suffixName);
            else command = commands::node::dynamic::ensureInsertNodeCommand(className, suffixName);
            commands::ngRunCommand(command, ctx);
            suffixName = {};
        }
    };
    create("Node", translatedNodeLabel("Node"));
    create("Mask", translatedNodeLabel("Mask"));
    create("Composite", translatedNodeLabel("Composite"));
    create("SimplePhysics", translatedNodeLabel("Simple Physics"));
    create("MeshGroup", translatedNodeLabel("Mesh Group"));
    create("DynamicComposite", translatedNodeLabel("Dynamic Composite"));
    create("PathDeformer", translatedNodeLabel("Path Deformer"));
    create("GridDeformer", translatedNodeLabel("Grid Deformer"));
    create("DepthRigRoot", translatedNodeLabel("Depth Rig Root"));
    create("DepthBone", translatedNodeLabel("Depth Bone"));
    create("Camera", translatedNodeLabel("Camera"));
}

void incNodeActionsPopup(const char* title, NodeRef n, bool isRoot, bool icon) {
    if (title && !ImGui::BeginPopup(title)) return;
    auto ctx = context(n);
    auto selected = project::incSelectedNodes();
    const auto label = [icon]<std::size_t N>(const char (&name)[N]) {
        const auto sourceName = compat::RetainedString::fromStaticLiteral(name);
        const auto key = icon ? nodeActionToIcon<true>(sourceName) : nodeActionToIcon<false>(sourceName);
        return core::translation::ngTranslateCString(key.view());
    };
    const auto tip = [icon]<std::size_t N>(const char (&name)[N]) { if (icon) ngTooltip(tr(name)); };
    if (ImGui::BeginMenu(label("Add").data())) { ngAddNodeMenu(); ImGui::EndMenu(); }
    tip("Add");
    if (ImGui::BeginMenu(label("Insert").data())) { ngInsertNodeMenu(); ImGui::EndMenu(); }
    tip("Insert");
    if (!isRoot) {
        if (std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(n) && !project::incArmedParameter())
            if (ImGui::MenuItem(label("Edit Mesh").data(), "", false, true)) run<NodeCommand::VertexMode>(ctx);
        tip("Edit Mesh");
        if (ImGui::MenuItem((compat::ngRequireObject(n)->getEnabled() ? label("Hide") : label("Show")).data()))
            run<NodeCommand::ToggleVisibility>(ctx);
        if (icon) {
            if (compat::ngRequireObject(n)->getEnabled()) tip("Hide");
            else tip("Show");
        }
        if (ImGui::MenuItem(label("Delete").data(), "", false, !isRoot)) run<NodeCommand::DeleteNode>(ctx);
        tip("Delete");
        if (ImGui::MenuItem(label("Copy").data(), "", false, true)) run<NodeCommand::CopyNode>(ctx);
        tip("Copy");
    }
    if (ImGui::MenuItem(label("Paste").data(), "", false, !commands::node::base::clipboardNodes.empty()))
        run<NodeCommand::PasteNode>(ctx);
    tip("Paste");
    if (ImGui::MenuItem(label("Reload").data(), "", false, true)) run<NodeCommand::ReloadNode>(ctx);
    tip("Reload");
    if (!isRoot) {
        if (ImGui::BeginMenu(label("More Info").data())) {
            if (selected.size() > 1) {
                for (auto sn : selected) {
                    const auto idPattern = tr("%s ID: %u");
                    const auto name = nodeName(sn);
                    const auto uuid = nodeUuid(sn);
                    ngText(format(idPattern, name, uuid));
                    if (auto part = std::dynamic_pointer_cast<ext::nodes::ExPart>(sn)) {
                        const auto layerPattern = tr("%s Layer: %s");
                        const auto partName = nodeName(part);
                        ngText(format(layerPattern, partName, part->layerPath));
                    }
                }
            } else {
                const auto idPattern = tr("ID: %u");
                const auto uuid = nodeUuid(n);
                ngText(format(idPattern, uuid));
                if (auto part = std::dynamic_pointer_cast<ext::nodes::ExPart>(n)) {
                    const auto layerPattern = tr("Layer: %s");
                    ngText(format(layerPattern, part->layerPath));
                }
            }
            ImGui::EndMenu();
        }
        tip("More Info");
        if (ImGui::BeginMenu(label("Parameter Usage").data())) {
            bool anyFound = false;
            const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
            for (auto parameter : parameters) {
                bool found = false;
                const auto bindings = compat::ngRequireObject(parameter)->bindings;
                for (auto binding : bindings) {
                    const auto targetNode = compat::ngRequireObject(binding)->getTarget().target.lock();
                    if (targetNode && targetNode == n) {
                        found = true; break;
                    }
                }
                if (found) {
                    anyFound = true;
                    if (ImGui::MenuItem(cstring(compat::ngRequireObject(parameter)->getName().view()).data(),
                            nullptr, false, true)) {
                        auto pctx = std::allocate_shared<commands::Context>(
                            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
                        pctx->puppet(project::incActivePuppet()); pctx->parameters({parameter});
                        namespace edit = commands::parameter::paramedit;
                        commands::ngCmd<edit::ParameditCommand::ToggleParameterArm>(pctx);
                    }
                }
            }
            if (!anyFound) ngText(tr("No parameter uses this node"));
            ImGui::EndMenu();
        }
        tip("Parameter Usage");
        if (ImGui::MenuItem(label("Recalculate origin").data(), "", false, true)) run<NodeCommand::CentralizeNode>(ctx);
        tip("Recalculate origin");
        auto fromType = commands::node::base::ngGetCommonNodeType(project::incSelectedNodes());
        const auto conversions = commands::node::base::conversionMap();
        if (conversions.contains(fromType.view())) {
            if (ImGui::BeginMenu(label("Convert To...").data())) {
                if (!selected.empty()) ctx->nodes(selected);
                const auto currentConversions = commands::node::base::conversionMap();
                const auto conversionValues = currentConversions.findRetained(fromType.view());
                if (!conversionValues) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/nodes.d"), 251);
                const auto convertedTypes = *conversionValues;
                for (auto toType : convertedTypes) {
                    ngText(utils::incTypeIdToIcon(toType.view())); ImGui::SameLine(0, 2);
                    if (ImGui::MenuItem(core::translation::ngTranslateCString(toType.view()).data(), "", false, true))
                        commands::ngRunCommand(commands::node::dynamic::ensureConvertToCommand(toType), ctx);
                }
                ImGui::EndMenu();
            }
            tip("Convert To ...");
        }
    }
    if (title) ImGui::EndPopup();
}
}
