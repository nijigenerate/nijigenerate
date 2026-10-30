#include "output_tree.hpp"
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/core/input_events.hpp>
#include <nijigenerate/panels/nodes_menu.hpp>
#include <nijigenerate/panels/inspector/base.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nodes/part.hpp>
#include <algorithm>
#include <bit>

namespace nijigenerate::widgets::output {
using namespace project;
using namespace core::selector;
using namespace panels::parameters;
using core::translation::ngTranslateRetained;
using core::translation::ngTranslateCString;
namespace {
std::string resourceLabel(Resource resource) {
    const auto icon = utils::ngTypeIdToIconSource(compat::RetainedString(
        compat::ngRequireObject(resource)->typeId()));
    const auto name = resource->name();
    return detail::allocateOutputMemory([&] { return std::string(icon.view()) + std::string(name.view()); });
}
std::string resourceId(Resource resource) {
    const auto name = compat::ngRequireObject(resource)->name();
    return detail::allocateOutputMemory([&] { return "###" + std::string(name.view()); });
}
void selectParameter(Parameter parameter) {
    if(incParamInSelection(parameter)) {
        if(incSelectedParams().size()>1) {
            if(ImGui::GetIO().KeyCtrl) incRemoveSelectParam(parameter);else incSelectParam(parameter);
        }
    } else if(ImGui::GetIO().KeyCtrl) incAddSelectParam(parameter);else incSelectParam(parameter);
}
template<class T> T dmin(T a,T b) { return b<a?b:a; }
template<class T> T dmax(T a,T b) { return a<b?b:a; }

float tripleIconSize(int size) {
    static_assert(sizeof(int)==sizeof(std::int32_t));
    return static_cast<float>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(size)*3u));
}
}
IconTreeOutput::IconTreeOutput(std::shared_ptr<TreeStore> store,CommandIssuerPtr issuer)
    : ListOutput(std::move(store),std::move(issuer)) { IconSize=48; }
ImGuiTreeNodeFlags IconTreeOutput::setFlag(Resource resource) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto flags=ListOutput::setFlag(resource);
    if(layout().contains(compat::ngRequireObject(resource)->uuid()) && layout().at(resource->uuid()).folded)
        flags&=~ImGuiTreeNodeFlags_DefaultOpen;
    return flags;
}
void IconTreeOutput::drawNode(Resource res,Node node,ImVec2& minimum,ImVec2& maximum,bool& hovered) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const bool selected=incNodeInSelection(node);
    const bool root=!compat::ngRequireObject(node)->parentRef() ||
        nicxlive::core::ngResourceObjectEquals(node,compat::ngRequireObject(incActivePuppet())->root);
    auto spacing=ImGui::GetStyle().ItemSpacing;
    if(!nodeIncluded().contains(res)) detail::setTransparency(0.5f,0.5f);
    auto select=[&](Node target) {
        if(selected) {
            if(incSelectedNodes().size()>1) {
                if(ImGui::GetIO().KeyCtrl) incRemoveSelectNode(target);else incSelectNode(target);
                activeInspector.reset();
            }
        } else {
            if(ImGui::GetIO().KeyCtrl) incAddSelectNode(target);else incSelectNode(target);
            activeInspector.reset();
        }
    };
    if(root && showRootThumb) {
        spacing=ImGui::GetStyle().ItemSpacing;ImGui::GetStyle().ItemSpacing={0,0};
        if(ImGui::Selectable(detail::outputCString(resourceId(res)).data(),selected,
                ImGuiSelectableFlags_AllowDoubleClick,{0,tripleIconSize(IconSize)})) select(node);
        hovered=ImGui::IsItemHovered();ImGui::SetItemAllowOverlap();ImGui::SameLine();minimum=ImGui::GetItemRectMin();
        const auto padding=ImGui::GetStyle().FramePadding.y;ImGui::GetStyle().FramePadding.y=1;
        if(!snapshot) snapshot=backend::snapshot::Snapshot::get(incActivePuppet());
        auto owner=std::static_pointer_cast<IconTreeOutput>(shared_from_this());
        compat::ngRequireObject(snapshot)->onCaptured(backend::snapshot::Snapshot::CaptureCallback::fromContext(owner,
            [](void* context,std::shared_ptr<nicxlive::core::Texture> texture) {
                static_cast<IconTreeOutput*>(context)->rootThumbTex=std::move(texture);
            }));
        compat::ngRequireObject(snapshot)->capture();
        if(rootThumbTex) ngTextureSlotUntitled("ICON",rootThumbTex,{tripleIconSize(IconSize),tripleIconSize(IconSize)},1,ImGuiWindowFlags_NoInputs,selected);
        else ImGui::Dummy({tripleIconSize(IconSize),tripleIconSize(IconSize)});
        ImGui::GetStyle().FramePadding.y=padding;
        if(isNodeClicked(node)) select(node);
        maximum=ImGui::GetItemRectMax();
        ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(compat::RetainedString(res->name()))));
    } else if(!root && node->typeId()=="Part") {
        spacing=ImGui::GetStyle().ItemSpacing;ImGui::GetStyle().ItemSpacing={0,0};
        auto part=std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node);
        if(ImGui::Selectable(detail::outputCString(resourceId(res)).data(),selected,
                ImGuiSelectableFlags_AllowDoubleClick,{0,float(IconSize)})) select(node);
        hovered=ImGui::IsItemHovered();ImGui::SetItemAllowOverlap();ImGui::SameLine();minimum=ImGui::GetItemRectMin();
        const auto padding=ImGui::GetStyle().FramePadding.y;ImGui::GetStyle().FramePadding.y=1;
        ngTextureSlotUntitled("ICON",part->textures[0],{float(IconSize),float(IconSize)},1,ImGuiWindowFlags_NoInputs,selected);
        ImGui::GetStyle().FramePadding.y=padding;
        if(isNodeClicked(res)) select(node);
        maximum=ImGui::GetItemRectMax();
        ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(compat::RetainedString(res->name()))));
    } else {
        minimum=ImGui::GetItemRectMin();
        if(ImGui::Selectable(detail::outputCString(resourceLabel(res)).data(),selected,
                ImGuiSelectableFlags_AllowDoubleClick,{0,20})) select(node);
        maximum=ImGui::GetItemRectMax();hovered=ImGui::IsItemHovered();
    }
    if(!nodeIncluded().contains(res)) ImGui::PopStyleColor(6);
    setNodeDragSource(node);setNodeReparentDragTarget(node);
    if(root || node->typeId()=="Part") ImGui::GetStyle().ItemSpacing=spacing;
}
bool IconTreeOutput::drawParameter(Resource res,ImVec2& minimum,ImVec2& maximum,bool& hovered) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto parameter=to<nicxlive::core::param::Parameter>(res);
    [[maybe_unused]] const bool active=nicxlive::core::ngResourceObjectEquals(incArmedParameter(),parameter);
    const bool selected=incParamInSelection(parameter);
    bool opened=false;
    if(auto group=std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
        minimum=ImGui::GetItemRectMin();
        const auto label = detail::outputCString(resourceLabel(res));
        const auto groupSelected = incParamInSelection(group);
        if(ImGui::Selectable(label.data(),groupSelected,ImGuiSelectableFlags_AllowDoubleClick,{0,20})) selectParameter(group);
        setParamDragTarget(group);maximum=ImGui::GetItemRectMax();
    } else {
        minimum=ImGui::GetItemRectMin();ImGui::PushID(parameter.get());
        if(ImGui::Selectable(detail::outputCString(compat::ngRequireObject(res)->name().view()).data(),selected,
                ImGuiSelectableFlags_AllowDoubleClick,{0,16})) selectParameter(parameter);
        if(isNodeClicked(res,ImGuiMouseButton_Right)) opened=true;
        setParamDragSource(parameter);
        if(ngController<4.0f,4.0f,3.0f,1.0f>("###CONTROLLER",parameter,{float(IconSize),float(IconSize)},false,grabParam.view())) {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if(grabParam.empty()) grabParam=compat::RetainedString(parameter->getName());
            } else grabParam=compat::RetainedString::emptyLiteral();
        }
        hovered=ImGui::IsItemHovered();maximum=ImGui::GetItemRectMax();
        ngTooltip(compat::RetainedStringSlice(ngTranslateRetained(compat::RetainedString(res->name()))));ImGui::PopID();
    }
    return opened;
}
void IconTreeOutput::drawBinding(Resource res,ImVec2& minimum,ImVec2& maximum,bool& hovered) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto binding=to<nicxlive::core::param::ParameterBinding>(res);
    bool active=false;
    if(incArmedParameter()) {
        auto parameter=compat::ngRequireObject(incArmedParameter());
        const auto bindings=parameter->bindings;
        if(std::find_if(bindings.begin(),bindings.end(),[&](auto candidate) {
            return nicxlive::core::ngResourceObjectEquals(candidate,binding);
        })!=bindings.end()) active=commands::binding::ngBindingIsSetAt(binding,incParamPoint());
    }
    minimum=ImGui::GetItemRectMin();
    const auto name = detail::getTransformText(compat::RetainedString(compat::ngRequireObject(res)->name()));
    const auto label = detail::allocateOutputMemory([&] {
        return std::string(active ? "\uE5DE" : "") + std::string(name.view()) + (active ? "\uE5DF" : "");
    });
    ImGui::Selectable(detail::outputCString(label).data(),false,ImGuiSelectableFlags_None,{0,16});
    hovered=ImGui::IsItemHovered();maximum=ImGui::GetItemRectMax();
}
void IconTreeOutput::drawTreeItem(Resource res) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    Node node;bool hovered=false,menuOpened=false;ImVec2 minimum,maximum;ImGui::SameLine();
    switch(compat::ngRequireObject(res)->type()) {
    case ResourceType::Node:node=to<nicxlive::core::nodes::Node>(res);drawNode(res,node,minimum,maximum,hovered);break;
    case ResourceType::Parameter:menuOpened=drawParameter(res,minimum,maximum,hovered);break;
    case ResourceType::Binding:drawBinding(res,minimum,maximum,hovered);break;
    default:minimum=ImGui::GetItemRectMin();maximum=ImGui::GetItemRectMax();break;
    }
    const bool itemHovered=ImGui::IsItemHovered();
    constexpr auto popup="##IconTreeViewNodeView",menu="##IconTreeViewNodeMenu";
    menuOpened=menuOpened || (itemHovered && isNodeClicked(res,ImGuiMouseButton_Right));
    if(ngSelectorResourceObjectEquals(popupOpened,res)) {
        constexpr auto flags=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize|
            ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings;
        if(ImGui::Begin(popup,nullptr,flags)) {
            hovered|=ngSelectorResourceObjectEquals(popupOpened,res) && detail::isWindowHovered();
            if(!contentsDrawn.contains(res->uuid())) {
                bool invalid=!activeInspector;
                if(activeInspector) { const auto targets=activeInspector->getTargets();
                    invalid=std::find_if(targets.begin(),targets.end(),[&](auto target) {
                        return nicxlive::core::ngResourceObjectEquals(target,node);
                    })==targets.end(); }
                const bool selected=node && incNodeInSelection(node);
                if(!selected || invalid) {
                    if(!selected) incSelectNode(node);
                    activeInspector=panels::inspector::ngNodeInspector(selected?
                        panels::inspector::Selection<nicxlive::core::nodes::Node>(incSelectedNodes()):
                        panels::inspector::Selection<nicxlive::core::nodes::Node>{node});
                }
                showContents(res);contentsDrawn[res->uuid()]=true;
            }
            detail::adjustWindowPos<detail::PopupSide::HorzLarger>(minimum,maximum,ImGui::FindWindowByName(popup));
        }
        ImGui::End();
        if(node && popupOpened) {
            const bool root=!node->parentRef() ||
                nicxlive::core::ngResourceObjectEquals(node,compat::ngRequireObject(incActivePuppet())->root);
            if(ImGui::Begin(menu,nullptr,flags)) {
                hovered|=ngSelectorResourceObjectEquals(popupOpened,res) && detail::isWindowHovered();
                panels::nodes::incNodeActionsPopup(nullptr,node,root,true);
                if(root && ImGui::MenuItem(ngTranslateCString("\uE84E").data(),nullptr,false,true)) {
                    showRootThumb=!showRootThumb;
                    if(!showRootThumb && snapshot) { compat::ngRequireObject(snapshot)->release();snapshot.reset(); }
                }
                detail::adjustWindowPos<detail::PopupSide::VertLarger>(minimum,maximum,ImGui::FindWindowByName(menu));
            }
            ImGui::End();
        }
        if(!hovered && mouseDown && !prevMouseDown) {
            popupOpened.reset();ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left]=false;
            ImGui::GetIO().MouseClicked[ImGuiMouseButton_Right]=false;
        }
    }
    if(menuOpened && !popupOpened) popupOpened=res;
    if(itemHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if(!layout().contains(res->uuid())) {
            layout().try_emplace(res->uuid());layout().at(res->uuid()).nextInHorizontal=true;
        } else layout().at(res->uuid()).nextInHorizontal=!layout().at(res->uuid()).nextInHorizontal;
    }
}
void IconTreeOutput::onUpdate() {
    auto owner=weak_from_this().lock();if(nodes().empty()) return;
    mouseDown=ImGui::IsAnyMouseDown();ImGui::PushID(this);contentsDrawn.clear();
    bool popupVisited=!popupOpened;
    using Parents=compat::Slice<std::uint32_t>;
    const auto traverse=[&](auto&& recurse, Resource res,bool& previous,Parents& parents) -> ImRect {
        const auto available=ngAvailableSpace();if(ngSelectorResourceObjectEquals(popupOpened,res)) popupVisited=true;
        const auto flags=setFlag(res);
        const bool horizontal=layout().contains(compat::ngRequireObject(res)->uuid()) &&
            layout().at(res->uuid()).nextInHorizontal;
        if(horizontal || previous) {
            if(previous) { ImGui::EndChild();ImGui::SameLine();parents.resize(parents.size()-1); }
            layout().try_emplace(res->uuid());
            const auto item=layout().at(res->uuid());
            const auto uuid = res->uuid();
            const auto childId = detail::allocateOutputMemory([&] { return "##horz" + std::to_string(uuid); });
            ImGui::BeginChild(detail::outputCString(childId).data(),
                {static_cast<float>(dmax(IconSize*1.5,static_cast<double>(item.bounds.Max.x-item.bounds.Min.x))),
                    item.bounds.Max.y-item.bounds.Min.y},
                ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            parents.append(res->uuid());
        }
        const bool opened=ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(res->uuid())),flags,"");
        ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(res->uuid())));drawTreeItem(res);ImGui::PopID();
        const auto resultMinimum=ImGui::GetItemRectMin();
        const auto resultMaximum=ImGui::GetItemRectMax();
        ImRect result(resultMinimum,resultMaximum);
        if(result.Max.x>result.Min.x+IconSize*1.5)
            result.Max.x=static_cast<float>(result.Min.x+IconSize*1.5);
        if(opened) {
            if(children().contains(res)) {
                bool subHorizontal=false;auto subParents=parents;
                const auto childSnapshot=children().at(res);
                for(auto child:childSnapshot) {
                    setNodeReorderDragTarget(child,!subHorizontal);
                    const auto rect=recurse(recurse, child,subHorizontal,subParents);
                    result.Min.x=dmin(result.Min.x,rect.Min.x);result.Min.y=dmin(result.Min.y,rect.Min.y);
                    result.Max.x=dmax(result.Max.x,rect.Max.x);result.Max.y=dmax(result.Max.y,rect.Max.y);
                }
                const auto childEndCount=subParents.size()-parents.size();
                for(std::size_t i=0;i<childEndCount;++i) ImGui::EndChild();
                if(layout().contains(res->uuid())) layout().at(res->uuid()).folded=false;
            }
            ImGui::TreePop();
        }
        if(horizontal || previous) {
            layout().try_emplace(res->uuid());
            if(result.Min.y!=result.Max.y) {
                layout().at(res->uuid()).bounds=result;layout().at(res->uuid()).scrolledOut=false;
            } else layout().at(res->uuid()).scrolledOut=true;
        }
        if(!opened && children().contains(res)) {
            const bool outside=result.Min.y==result.Max.y || result.Min.y>available.y || result.Min.y<0;
            if(!outside) {
                layout().try_emplace(res->uuid());
                if(!layout().at(res->uuid()).folded)layout().at(res->uuid()).folded=true;
            }
        } else if(opened && layout().contains(res->uuid())) {
            if(layout().at(res->uuid()).folded)layout().at(res->uuid()).folded=false;
        }
        const auto parentSnapshot=parents;
        for(auto uuid:parentSnapshot) if(uuid!=std::numeric_limits<std::uint32_t>::max()) {
            layout().try_emplace(uuid);
            if(!layout().at(uuid).scrolledOut) {
                auto bounds=layout().at(uuid).bounds;
                bounds.Min.x=dmin(result.Min.x,bounds.Min.x);bounds.Min.y=dmin(result.Min.y,bounds.Min.y);
                bounds.Max.x=dmax(result.Max.x,bounds.Max.x);bounds.Max.y=dmax(result.Max.y,bounds.Max.y);
                layout().at(uuid).bounds=bounds;
            }
        }
        previous=horizontal;return result;
    };
    const auto spacing=ImGui::GetStyle().IndentSpacing;
    ImGui::GetStyle().IndentSpacing=static_cast<float>(ImGui::GetStyle().IndentSpacing/2.5);
    const auto rootSnapshot=roots();
    for(auto root:rootSnapshot) { bool horizontal=false;Parents parents;traverse(traverse, root,horizontal,parents);
        const auto parentEndCount=parents.size();
        for(std::size_t i=0;i<parentEndCount;++i) ImGui::EndChild(); }
    if(!popupVisited) { popupOpened.reset();mouseDown=false;prevMouseDown=false; }
    const auto keys=layout().keys();
    for(auto key:keys) { const auto nodeSnapshot=nodes();Resource found;
        for(auto resource:nodeSnapshot) if(compat::ngRequireObject(resource)->uuid()==key) { found=resource;break; } }
    ImGui::GetStyle().IndentSpacing=spacing;ImGui::PopID();prevMouseDown=mouseDown;
}
void IconTreeOutput::setResources(SelectorArray<Resource> resources) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ListOutput::setResources(std::move(resources));popupOpened.reset();mouseDown=false;prevMouseDown=false;
}
void IconTreeOutput::reset() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    layout().clear();prevMouseDown=false;mouseDown=false;popupOpened.reset();
    if(snapshot) compat::ngRequireObject(snapshot)->release();snapshot.reset();ListOutput::reset();
}
}
