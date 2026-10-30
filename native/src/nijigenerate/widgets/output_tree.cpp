#include "output_tree.hpp"
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/commands/parameter/base.hpp>
#include <nijigenerate/panels/nodes_menu.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <new>
#include <source_location>
#include <compat/source_allocator.hpp>

namespace nijigenerate::widgets::output {
using namespace core::selector;
using namespace panels::parameters;
using namespace project;
using core::translation::ngTranslateCString;
namespace {
void errorDialog(const compat::ThrowableException& error) {
    const auto title=ngTranslateCString("Error");
    dialog::ngDialog(dialog::DialogTag::retained(title.data(),title.owner),
        dialog::DialogText(error.msg));
}
std::string label(Resource res,bool active=false,bool noIcon=false) {
    compat::RetainedString icon;
    if (!noIcon) icon = utils::ngTypeIdToIconSource(compat::RetainedString(
        compat::ngRequireObject(res)->typeId()));
    const auto name = compat::ngRequireObject(res)->name();
    return detail::allocateOutputMemory([&] {
        return std::string(icon.view()) + (active ? "\uE3FA" : "") + std::string(name.view());
    });
}
bool bindingActive(Resource res) {
    auto binding=to<nicxlive::core::param::ParameterBinding>(res);
    if (!incArmedParameter()) return false;
    auto param=compat::ngRequireObject(incArmedParameter());
    const auto bindings=param->bindings;
    return std::find_if(bindings.begin(),bindings.end(),[&](auto candidate) {
        return nicxlive::core::ngResourceObjectEquals(candidate,binding);
    })!=bindings.end() &&
        commands::binding::ngBindingIsSetAt(binding,incParamPoint());
}
void selectParameter(Parameter param) {
    const bool selected=incParamInSelection(param);
    if (selected) {
        if (incSelectedParams().size()>1) {
            if (ImGui::GetIO().KeyCtrl) incRemoveSelectParam(param); else incSelectParam(param);
        }
    } else if (ImGui::GetIO().KeyCtrl) incAddSelectParam(param); else incSelectParam(param);
}
}
ListOutput::ListOutput(std::shared_ptr<TreeStore> store,CommandIssuerPtr issuer)
    : self(std::move(store)),panel(issuer) {}
void ListOutput::setNodeReorderDragTarget(Resource res,bool horizontal,float width) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (compat::ngRequireObject(res)->type()!=ResourceType::Node) return;
    const auto selected=incSelectedNodes();
    const auto available=ngAvailableSpace();
    width=horizontal?(width<0?static_cast<float>(IconSize*1.5):width):4;
    const float height=horizontal?4:(available.y<IconSize?static_cast<float>(IconSize):available.y);
    const float padding=ImGui::GetStyle().FramePadding.y;
    ImGui::GetStyle().FramePadding.y=0;
    const auto uuid = res->uuid();
    const auto target = detail::allocateOutputMemory([&] { return "###TARGET" + std::to_string(uuid); });
    ImGui::InvisibleButton(detail::outputCString(target).data(), {width, height});
    ImGui::GetStyle().FramePadding.y=padding;
    auto node=to<nicxlive::core::nodes::Node>(res);
    if (ImGui::BeginDragDropTarget()) {
        if (auto payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
            auto payloadNode=ngNodeFromDragDropPayload(payload);
            auto parent=compat::ngRequireObject(compat::ngRequireObject(node)->parentRef());
            const auto children=parent->children;
            const auto found=std::find_if(children.begin(),children.end(),[&](auto child) {
                return nicxlive::core::ngResourceObjectEquals(child,node);
            });
            const auto index=found==children.end()?std::numeric_limits<std::size_t>::max():
                static_cast<std::size_t>(found-children.begin());
            try {
                if (selected.size()>1) actions::node::incMoveChildrenWithHistory(selected,parent,index);
                else actions::node::incMoveChildWithHistory(payloadNode,parent,index);
            } catch(const compat::ThrowableException& ex) { errorDialog(ex); }
        }
        ImGui::EndDragDropTarget();
    }
}
void ListOutput::setNodeDragSource(Node node) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const bool root=!compat::ngRequireObject(node)->parentRef();
    const auto selected=incSelectedNodes();
    if (root) return;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ngSetNodeDragDropPayload(node);
        if (selected.size()>1) ngDragdropNodeList(selected); else ngDragdropNodeList(node);
        ImGui::EndDragDropSource();
    }
}
void ListOutput::setNodeReparentDragTarget(Node node) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ImGui::BeginDragDropTarget()) return;
    const auto selected=incSelectedNodes();
    if (auto payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
        auto payloadNode=ngNodeFromDragDropPayload(payload);
        try {
            if (selected.size()>1) actions::node::incMoveChildrenWithHistory(selected,node,0);
            else actions::node::incMoveChildWithHistory(payloadNode,node,0);
        } catch(const compat::ThrowableException& ex) { errorDialog(ex); }
    }
    ImGui::EndDragDropTarget();
}
void ListOutput::setParamDragSource(Parameter parameter) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
    if (!dragDropData) {
        dragDropOwner_ = std::allocate_shared<ParamDragDropData>(
            nicxlive::core::d_runtime::SourceAllocator<ParamDragDropData>{});
        dragDropData=dragDropOwner_.get();
    }
    dragDropData->param=parameter;
    ImGui::SetDragDropPayload("_PARAMETER",&dragDropData,sizeof(dragDropData),ImGuiCond_Always);
    ngText(compat::RetainedStringSlice(compat::RetainedString(
        compat::ngRequireObject(dragDropData->param)->getName())));
    ImGui::EndDragDropSource();
}
void ListOutput::setParamDragTarget(std::shared_ptr<ext::ExParameterGroup> group) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ngBeginDragDropFake();
    auto peek=ImGui::AcceptDragDropPayload("_PARAMETER",ImGuiDragDropFlags_AcceptPeekOnly|ImGuiDragDropFlags_SourceAllowNullID);
    if (peek && peek->Data && ImGui::BeginDragDropTarget()) {
        if (auto payload=ImGui::AcceptDragDropPayload("_PARAMETER")) {
            auto data=*static_cast<ParamDragDropData* const*>(payload->Data);
            commands::parameter::base::incMoveParameter(data->param,group);
            auto root=compat::ngRequireObject(incActivePuppet())->root;
            compat::ngRequireObject(root)->notifyChange(root,nicxlive::core::nodes::NotifyReason::StructureChanged);
        }
        ImGui::EndDragDropTarget();
    }
    ngEndDragDropFake();
}
void ListOutput::showContents(Resource res) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const ImVec2 size(20,20);
    auto pin=[&] {
        if (ngButtonColored("\uE763",size)) if (auto issuer=panel) {
            issuer->addPopup(res);popupOpened.reset();
        }
    };
    if (compat::ngRequireObject(res)->type()==ResourceType::Node) {
        pin();auto node=to<nicxlive::core::nodes::Node>(res);
        ImGui::SameLine();
        if (ngButtonColored("\uE8B8",size)) ImGui::OpenPopup("NodeActionsPopup2");
        panels::nodes::incNodeActionsPopup("NodeActionsPopup2",node,!node->parentRef(),true);
        ImGui::SameLine();ImGui::Text(detail::outputCString(res->name().view()).data());
        detail::onNodeView(node,activeInspector);
    } else if (res->type()==ResourceType::Parameter) {
        auto param=to<nicxlive::core::param::Parameter>(res);
        if (auto group=std::dynamic_pointer_cast<ext::ExParameterGroup>(param)) {
            if (group->children.empty()) {
                const auto newGroup=ngTranslateCString("New Parameter Group");
                if (ImGui::MenuItem(newGroup.data(),"",false,true)) {
                    auto puppet=std::dynamic_pointer_cast<ext::ExPuppet>(incActivePuppet());
                    commands::parameter::base::incCreateParamGroup(puppet?static_cast<int>(puppet->groups.size()):0);
                    auto root=compat::ngRequireObject(incActivePuppet())->root;
                    compat::ngRequireObject(root)->notifyChange(root,nicxlive::core::nodes::NotifyReason::StructureChanged);
                }
                ImGui::Separator();incParameterMenuContents(compat::ngRequireObject(incActivePuppet())->parameters);
            } else incParameterGropuMenuContents(group);
        } else {
            pin();ImGui::SameLine();
            incParameterViewEditButtons<false,true>(res->index,param,compat::ngRequireObject(incActivePuppet())->parameters,true);
            ImGui::SameLine();ImGui::Text(detail::outputCString(res->name().view()).data());
            detail::onParameterView(res->index,param);
        }
    } else if (res->type()==ResourceType::Binding) {
        using namespace nicxlive::core::param;
        auto binding=to<ParameterBinding>(res);
        commands::binding::SelectedBindings bindings;bindings[binding->getTarget()]=binding;
        if(auto value=std::dynamic_pointer_cast<ValueParameterBinding>(binding))
            incBindingMenuContents(value->getParameter(),bindings);
        else if(auto value=std::dynamic_pointer_cast<DeformationParameterBinding>(binding))
            incBindingMenuContents(value->getParameter(),bindings);
    }
}
ImGuiTreeNodeFlags ListOutput::setFlag(Resource res) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return (children().contains(res)?0:ImGuiTreeNodeFlags_Leaf)|ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_OpenOnArrow;
}
void ListOutput::drawTreeItem(Resource res) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const bool isNode=compat::ngRequireObject(res)->type()==ResourceType::Node;
    bool selected=false,noIcon=false;Node node;
    ImGui::SameLine();
    if(isNode) {
        node=to<nicxlive::core::nodes::Node>(res);selected=incNodeInSelection(node);
        if(auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) {
            if(node->typeId()=="Part") {
                noIcon=true;ngTextureSlotUntitled("ICON",part->textures[0],{float(IconSize),float(IconSize)},1,ImGuiWindowFlags_NoInputs);
            }
            ImGui::SameLine();
        }
    }
    if(!nodeIncluded().contains(res)) ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    bool active=false;
    if(res->type()==ResourceType::Parameter) {
        auto param=to<nicxlive::core::param::Parameter>(res);
        active=nicxlive::core::ngResourceObjectEquals(incArmedParameter(),param);selected=incParamInSelection(param);
    } else if(res->type()==ResourceType::Binding) active=bindingActive(res);
    if(ImGui::Selectable(detail::outputCString(label(res,active,noIcon)).data(),selected,
            ImGuiSelectableFlags_AllowDoubleClick,{0,20})) {
        if(isNode) { node=to<nicxlive::core::nodes::Node>(res);incSelectNode(node); }
        else if(res->type()==ResourceType::Parameter) selectParameter(to<nicxlive::core::param::Parameter>(res));
    }
    if(isNode) { setNodeDragSource(node);setNodeReparentDragTarget(node); }
    const bool hovered=ImGui::IsItemHovered();
    constexpr auto popup="##TreeViewNodeView",menu="##TreeViewNodeMenu";
    if(hovered && (isNode||res->type()==ResourceType::Parameter) && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(popup);if(isNode) ImGui::OpenPopup(menu);
    }
    if(ImGui::BeginPopup(popup)) {
        if(!contentsDrawn.contains(res->uuid())) { showContents(res);contentsDrawn[res->uuid()]=true; }
        ImGui::EndPopup();
    }
    if(node) panels::nodes::incNodeActionsPopup(menu,node,!node->parentRef(),true);
    if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && panel) {
        const auto type = res->typeId();
        const auto uuid = res->uuid();
        const auto command = detail::allocateOutputMemory([&] {
            return compat::RetainedString(" " + std::string(type.view()) + "#" + std::to_string(uuid));
        });
        compat::ngRequireObject(panel)->setCommand(command);
    }
    if(!nodeIncluded().contains(res)) ImGui::PopStyleColor();
}
void ListOutput::onUpdate() {
    auto owner=weak_from_this().lock();
    if(nodes().empty()) return;
    ImGui::PushID(this);contentsDrawn.clear();
    const auto traverse=[&](auto&& recurse, Resource res) -> void {
        const auto flags=setFlag(res);
        const auto id=reinterpret_cast<void*>(static_cast<std::uintptr_t>(compat::ngRequireObject(res)->uuid()));
        const bool opened=ImGui::TreeNodeEx(id,flags,"");
        ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(res->uuid())));drawTreeItem(res);ImGui::PopID();
        if(opened) {
            if(children().contains(res)) { const auto childSnapshot=children().at(res);
                for(auto child:childSnapshot) { setNodeReorderDragTarget(child,true,128);recurse(recurse, child); }
            }
            ImGui::TreePop();
        }
    };
    const float spacing=ImGui::GetStyle().IndentSpacing;
    ImGui::GetStyle().IndentSpacing=static_cast<float>(ImGui::GetStyle().IndentSpacing/2.5);
    const auto rootSnapshot=roots();for(auto root:rootSnapshot) traverse(traverse, root);
    ImGui::GetStyle().IndentSpacing=spacing;ImGui::PopID();
}
void ListOutput::setResources(SelectorArray<Resource> resources) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::ngRequireObject(self)->setResources(std::move(resources));
}
void ListOutput::reset() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    focused.reset();contentsDrawn.clear();compat::ngRequireObject(self)->reset();
}
}
