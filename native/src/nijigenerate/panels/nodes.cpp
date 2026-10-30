#include "nodes.hpp"
#include "nodes_internal.hpp"
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/utils.hpp>
#include <timing.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <imgui_internal.h>
#include <bit>

namespace nijigenerate::panels::nodes {
void ngInitializeNodesPanelModule() {
    (void)detail::actionIconMap;
    panels::ngInitializePanelModule<NodesPanel>();
}
using namespace detail;
using namespace widgets;
using namespace project;
namespace {
bool blocked() { return api::mcp::task::ngMcpExternalCommandsBlocked(); }
void moveNode(ContextRef& ctx, NodeRef& parent, std::size_t index) {
    commands::ngCmd<NodeCommand::MoveNode>(ctx, parent, index);
}
}
void NodesPanel::treeSetEnabled(NodeHandle n, bool enabled) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(n)->setEnabled(enabled);
    auto children = compat::ngRequireObject(n)->children;
    for (auto child : children) treeSetEnabled(child, enabled);
}
template<bool isRoot> void NodesPanel::treeAddNode(NodeHandle& n) {
    const auto keepAlive = weak_from_this().lock();
    if (!nodeFilterValue(filterResult, [&] { return nodeUuid(n); }, 401)) return;
    auto ctx = context(n);
    ImGui::TableNextRow();
    auto& io = ImGui::GetIO();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    if (compat::ngRequireObject(n)->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    ImGui::TableSetColumnIndex(0); ImGui::SetNextItemWidth(8);
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(nodeUuid(n))), flags, "");
    ImGui::SameLine(0, 4);
    auto selectedNodes = incSelectedNodes();
    ImGui::PushID(std::bit_cast<std::int32_t>(nodeUuid(n)));
    const bool selected = incNodeInSelection(n);
    ImGui::BeginGroup(); ImGui::Indent(4);
    if constexpr (!isRoot) {
        if (compat::ngRequireObject(n)->getEnabled())
            ngText(utils::incTypeIdToIcon(compat::ngRequireObject(n)->typeId()));
        else ngTextDisabled(utils::incTypeIdToIcon(compat::ngRequireObject(n)->typeId()));
        if (ImGui::IsItemClicked()) run<NodeCommand::ToggleVisibility>(ctx);
    } else ngText("\xee\xa4\xac");
    ImGui::SameLine(0, 2);
    handleRangeSelect(n);
    if (ImGui::Selectable(isRoot ? core::translation::ngTranslateCString("Puppet").data()
            : cstring(nodeName(n).view()).data(), selected,
            ImGuiSelectableFlags_None, ImVec2(0, 0))) {
        // The authored switch has only a default arm, but still reads edit mode.
        (void)incEditMode();
        selectStateUpdate = true;
        if (!io.KeyShift) nextSelectState.lastClick = n;
        if (io.KeyCtrl && !io.KeyShift) toggleSelect(n);
        else if (!io.KeyCtrl && io.KeyShift) selectRange(n);
        else if (selected && selectedNodes.size() == 1) incFocusCamera(n);
        else incSelectNode(n);
    }
    trackingRenderedNode(n);
    if (followSelection && pendingFocus && selected) { ImGui::SetScrollHereY(.25f); pendingFocus = false; }
    if (!blocked() && ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("NodeActionsPopup");
    if (!blocked()) incNodeActionsPopup("NodeActionsPopup", n, isRoot);
    ImGui::EndGroup();
    if constexpr (!isRoot) {
        if (!blocked() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ngSetNodeDragDropPayload(n, ImGuiCond_Always);
            if (selectedNodes.size() > 1) ngDragdropNodeList(selectedNodes); else ngDragdropNodeList(n);
            ImGui::EndDragDropSource();
        }
    }
    ImGui::PopID();
    if (!blocked() && ImGui::BeginDragDropTarget()) {
        bool moved = false;
        compat::ngRunWithScopeExit([&] {
            const auto* payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE");
            if (payload) {
                auto pctx = context(ngNodeFromDragDropPayload(payload));
                moveNode(pctx, n, 0);
                if (open) ImGui::TreePop();
                moved = true;
            }
        }, [] { ImGui::EndDragDropTarget(); });
        if (moved) return;
    }
    if (open) {
        bool childMoved = false;
        const auto drawChildren = [&](NodeHandle& child, std::size_t i) {
            const auto childUuid = nodeUuid(child);
            const auto membership = filterResult;
            if (!membership.contains(childUuid)) return;
            if (!nodeFilterValue(filterResult, [&] { return nodeUuid(child); }, 525)) return;
            ImGui::PushID(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i)));
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            ImGui::InvisibleButton("###TARGET", ImVec2(128, 4));
            if (!blocked() && ImGui::BeginDragDropTarget()) {
                compat::ngRunWithScopeExit([&] {
                    const auto* payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE");
                    if (payload) {
                        auto pctx = context(ngNodeFromDragDropPayload(payload));
                        moveNode(pctx, n, i);
                        ImGui::PopID(); childMoved = true;
                    }
                }, [] { ImGui::EndDragDropTarget(); });
                if (childMoved) return;
            }
            ImGui::PopID(); treeAddNode(child);
        };
        auto children = compat::ngRequireObject(n)->children;
        if (revserseOrder) {
            for (std::size_t i = children.size(); i-- > 0;) {
                auto child = children[i];
                drawChildren(child, i);
                if (childMoved) break;
            }
        } else {
            for (std::size_t i = 0; i < children.size(); ++i) {
                auto child = children[i];
                drawChildren(child, i);
                if (childMoved) break;
            }
        }
        ImGui::TreePop();
        if (childMoved) return;
    }
}
bool NodesPanel::filterNodes(NodeHandle n) {
    const auto keepAlive = weak_from_this().lock();
    bool result = io::depthmap_psd::ngPsdDepthLowerString(compat::RetainedString(nodeName(n))).view().find(filter.view())
        != std::string_view::npos;
    auto children = compat::ngRequireObject(n)->children;
    for (auto child : children) result |= filterNodes(child);
    auto& destination = filterResult;
    const auto uuid = nodeUuid(n);
    destination[uuid] = result;
    return result;
}
void NodesPanel::onUpdate() {
    const auto keepAlive = weak_from_this().lock();
    if (incEditMode() == EditMode::ModelEdit && !incArmedParameter() &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)))
        if (core::input::ngShortcut("Ctrl+A")) incSelectAll();
    if (incEditMode() == EditMode::VertexEdit) { ngLabelOver(tr("In vertex edit mode..."), ImVec2(0, 0), true); return; }
    if (ImGui::BeginChild("NodesMain", ImVec2(0, -30), false)) {
        auto selected = incSelectedNodes();
        const std::uint32_t current = selected.empty() ? 0 : nodeUuid(selected[0]);
        if (current != lastSelUuid) { lastSelUuid = current; pendingFocus = true; }
        float scrollDelta = 0;
        const auto avail = ngAvailableSpace();
        const auto screenPos = ImGui::GetCursorScreenPos();
        const ImRect rect(screenPos, ImVec2(screenPos.x + avail.x, screenPos.y + avail.y));
        ngBeginDragDropFake();
        auto data = ImGui::AcceptDragDropPayload("_PUPPETNTREE", ImGuiDragDropFlags_AcceptPeekOnly | ImGuiDragDropFlags_SourceAllowNullID);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && data && data->Data) {
            const auto mouse = ImGui::GetMousePos();
            if (mouse.x > rect.Min.x && mouse.x < rect.Max.x) {
                const float speed = static_cast<float>((4 * 60) * nicxlive::core::deltaTime());
                if (mouse.y < rect.Min.y + 32 && mouse.y >= rect.Min.y) scrollDelta = -speed;
                if (mouse.y > rect.Max.y - 32 && mouse.y <= rect.Max.y) scrollDelta = speed;
            }
        }
        ngEndDragDropFake();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14);
        if (input("Node Filter", filter)) {
            filter = io::depthmap_psd::ngPsdDepthLowerString(filter);
            filterResult.clear();
        }
        ngTooltip(tr("Filter, search for specific nodes"));
        filterNodes(compat::ngRequireObject(incActivePuppet())->root);
        if (ImGui::BeginTable("NodesContent", 2, ImGuiTableFlags_ScrollX, ImVec2(0, 0), 0)) {
            ImGui::SetScrollY(ImGui::GetCurrentWindow()->Scroll.y + scrollDelta);
            ImGui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 0, 0);
            if (incEditMode() == EditMode::ModelEdit) {
                if (selectStateUpdate) { curSelectState = nextSelectState; curSelectState.state = SelectState::Init; selectStateUpdate = false; }
                startTrackingRenderedNodes(); ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                treeAddNode<true>(compat::ngRequireObject(incActivePuppet())->root);
                ImGui::PopStyleVar(); endTrackingRenderedNodes();
            }
            ImGui::EndTable();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) incSelectNode(nullptr);
        ImGui::PopStyleVar(); ImGui::PopStyleVar();
    }
    ImGui::EndChild(); ImGui::Separator(); ImGui::Spacing();
    if (incEditMode() == EditMode::ModelEdit) {
        auto selected = incSelectedNodes();
        ImGui::BeginDisabled(blocked());
        if (ngButtonColored("\xee\xa4\xae", ImVec2(24, 24))) {
            for (auto payloadNode : selected) actions::node::incDeleteChildWithHistory(payloadNode);
            incSelectNode(nullptr);
        }
        ngTooltip(tr("Delete selected nodes")); ImGui::EndDisabled(); ImGui::SameLine(0, 2);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const ImVec4 initial(nan, nan, nan, nan);
        if (ngButtonColored("\xee\x85\xa4##SortNodeOrder", ImVec2(24, 24), revserseOrder ? initial : ImVec4(.6f,.6f,.6f,1)))
            revserseOrder = !revserseOrder;
        ngTooltip(tr("Reverse Node Order")); ImGui::SameLine(0, 2);
        if (ngButtonColored("\xee\xa1\xba###FollowSelection", ImVec2(24, 24), followSelection ? initial : ImVec4(.6f,.6f,.6f,1)))
            followSelection = !followSelection;
        ngTooltip(tr("Follow Selection"));
        if (!blocked() && ImGui::BeginDragDropTarget()) {
            bool removed = false;
            compat::ngRunWithScopeExit([&] {
                const auto* payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE");
                if (payload) {
                    auto payloadNode = ngNodeFromDragDropPayload(payload);
                    if (selected.size() > 1) {
                        for (auto pn : selected) actions::node::incDeleteChildrenWithHistory(selected);
                        incSelectNode(nullptr);
                    } else {
                        if (incNodeInSelection(payloadNode)) incSelectNode(nullptr);
                        actions::node::incDeleteChildWithHistory(payloadNode);
                    }
                    removed = true;
                }
            }, [] { ImGui::EndDragDropTarget(); });
            if (removed) return;
        }
    }
}
}
