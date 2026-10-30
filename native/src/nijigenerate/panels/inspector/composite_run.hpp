#pragma once
#include "composite.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/mask_commands.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <resource.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dragdrop_mask_payload.hpp>
#include <nijigenerate/widgets/drag_float_fields.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
namespace nijigenerate::panels::inspector::composite {
namespace detail {
using MaskBinding=nicxlive::core::nodes::MaskBinding;
using MaskArray=nicxlive::core::common::SharedArray<MaskBinding>;
inline MaskArray duplicateMasks(const MaskArray& masks) {
    try {
        return masks.duplicate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    }
}
inline MaskArray allocateMaskArray(std::size_t length) {
    try {
        return MaskArray(length,MaskBinding{});
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"),1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"),1142);
    }
}
inline MaskArray concatenateMasks(MaskArray left,MaskArray right) {
    auto result=allocateMaskArray(left.size()+right.size());
    std::size_t index=0;
    for (const auto& value:left) result[index++]=value;
    for (const auto& value:right) result[index++]=value;
    return result;
}
inline MaskArray concatenateMasks(MaskArray left,const MaskBinding& middle,MaskArray right) {
    auto result=allocateMaskArray(left.size()+1+right.size());
    std::size_t index=0;
    for (const auto& value:left) result[index++]=value;
    result[index++]=middle;
    for (const auto& value:right) result[index++]=value;
    return result;
}
inline compat::RetainedString retainedText(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<commands::node::mask::NodeMaskCommand id,class... Args>
void runMask(std::shared_ptr<commands::Context> context,Args&&... args) {
    namespace source=commands::node::mask;
    using Id=source::NodeMaskCommand;
    static_assert(id==Id::AddMask || id==Id::RemoveMask || id==Id::ChangeMaskMode);
    using Actual=std::conditional_t<id==Id::AddMask,source::AddMaskCommand,
        std::conditional_t<id==Id::RemoveMask,source::RemoveMaskCommand,source::ChangeMaskModeCommand>>;
    constexpr auto key=id==Id::AddMask ? "NodeMaskCommand.AddMask" :
        id==Id::RemoveMask ? "NodeMaskCommand.RemoveMask" : "NodeMaskCommand.ChangeMaskMode";
    constexpr auto type=id==Id::AddMask ? "AddMaskCommand" :
        id==Id::RemoveMask ? "RemoveMaskCommand" : "ChangeMaskModeCommand";
    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands,id,context,
        {key,"NodeMaskCommand",type},std::forward<Args>(args)...);
}
template<class Array> decltype(auto) maskAt(Array& masks,std::size_t index,std::size_t line) {
    if(index>=masks.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index,masks.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/composite.d"),line);
    return masks[index];
}
inline std::shared_ptr<commands::Context> context(std::shared_ptr<TypedInspector<Node>> inspector,Selection<Node> nodes) {
    auto result=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    result->inspectors(commands::Context::Inspectors{inspector});
    result->nodes(static_cast<commands::Context::Nodes>(nodes)); return result;
}
inline bool sameMasks(const auto& left,const auto& right) {
    if(left.size()!=right.size()) return false;
    for(std::size_t i=0;i<left.size();++i)
        if(left[i].maskSrcUUID!=right[i].maskSrcUUID ||
            !nicxlive::core::ngResourceObjectEquals(left[i].maskSrc.lock(),right[i].maskSrc.lock()) ||
            left[i].mode!=right[i].mode) return false;
    return true;
}
}
template<class T> requires std::derived_from<T,Composite>
void NodeInspector<ModelEditSubMode::Layout,T>::run() {
    using namespace widgets;
    using core::translation::ngTranslateCString;
    const auto translated=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(text)));
    };
    using Id=commands::inspector::apply_node::InspectorNodeApplyCommand;
    using MaskId=commands::node::mask::NodeMaskCommand;
    using MaskMode=nicxlive::core::nodes::MaskingMode;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return;
    auto node=this->targets[0];
    auto context=[&] {
        auto result=std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        result->inspectors(commands::Context::Inspectors{self});
        result->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets)));
        return result;
    };
    if(ngBeginCategory(ngTranslateCString("Composite").data())) {
        ImGui::Spacing();
        const auto edit=[&]<Attribute attribute>(auto& property,Id id,auto editor) {
            const auto state=[&] {return property.get(self);};
            if(this->template shared<Descriptor<T,attribute>>(state,[&] {return editor(state);})) {
                const auto ctx=context();
                const auto current=compat::ngRequireObject(state());
                ngRunCompositePropertyCommand(id,ctx,std::cref(current->value));
            }
        };
        ngText(translated("Tint (Multiply)"));
        edit.template operator()<Attribute::Tint>(tint,Id::CompositeTint,[](auto state) {
            const auto current=compat::ngRequireObject(state());
            return ngColorEdit3Fields("###TINT",current->value.x,current->value.y,current->value.z);
        });
        ngText(translated("Tint (Screen)"));
        edit.template operator()<Attribute::ScreenTint>(screenTint,Id::CompositeScreenTint,[](auto state) {
            const auto current=compat::ngRequireObject(state());
            return ngColorEdit3Fields("###S_TINT",current->value.x,current->value.y,current->value.z);
        });
        ngText(translated("Blending"));
        edit.template operator()<Attribute::BlendingMode>(blendingMode,Id::CompositeBlendingMode,[&](auto state) {
            const auto previous=state()->value;
            if(ImGui::BeginCombo("###Blending",
                    core::translation::ngTranslateCString(ngCompositeBlendName(state()->value)).data())) {
                const std::pair<BlendMode,const char*> choices[]{
                    {BlendMode::Normal,"Normal"},{BlendMode::Multiply,"Multiply"},{BlendMode::Screen,"Screen"},
                    {BlendMode::Overlay,"Overlay"},{BlendMode::Darken,"Darken"},{BlendMode::Lighten,"Lighten"},
                    {BlendMode::ColorDodge,"Color Dodge"},{BlendMode::LinearDodge,"Linear Dodge"},{BlendMode::AddGlow,"Add (Glow)"},
                    {BlendMode::ColorBurn,"Color Burn"},{BlendMode::HardLight,"Hard Light"},{BlendMode::SoftLight,"Soft Light"},
                    {BlendMode::Subtract,"Subtract"},{BlendMode::Difference,"Difference"},{BlendMode::Exclusion,"Exclusion"},
                    {BlendMode::Inverse,"Inverse"},{BlendMode::DestinationIn,"Destination In"}};
                for(auto [mode,label]:choices) {
                    if(ImGui::Selectable(core::translation::ngTranslateCString(label).data(),state()->value==mode)) state()->value=mode;
                    if(mode==BlendMode::Inverse) ngTooltip(compat::RetainedStringSlice(
                        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                            "Inverts the color by a factor of the overlying color"))));
                }
                ImGui::EndCombo();
            }
            return previous!=state()->value;
        });
        ImGui::Spacing(); ngText(translated("Opacity"));
        edit.template operator()<Attribute::Opacity>(opacity,Id::CompositeOpacity,[](auto state) {
            const auto current=compat::ngRequireObject(state());
            return ImGui::SliderFloat("###Opacity",&current->value,0,1,"%0.2f");
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Masks").data()); ImGui::Spacing();
        ngText(translated("Threshold"));
        edit.template operator()<Attribute::Threshold>(threshold,Id::CompositeThreshold,[](auto state) {
            const auto current=compat::ngRequireObject(state());
            return ImGui::SliderFloat("###Threshold",&current->value,0,1,"%.2f");
        });
        ImGui::Spacing(); ngText(translated("Mask Sources"));
        if(ImGui::BeginListBox("###MaskSources",ImVec2(0,128))) {
            if(compat::ngRequireObject(node)->masks.empty()) ngText(translated("(Drag a Part or Mask Here)"));
            const auto count=node->masks.size();
            for(std::size_t i=0;i<count;++i) {
                auto maskBacking=node->masks;
                auto* masker=&detail::maskAt(maskBacking,i,157);
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::BeginPopup("###MaskSettings")) {
                    if(ImGui::BeginMenu(ngTranslateCString("Mode").data())) {
                        auto ctx=context();
                        if(ImGui::MenuItem(ngTranslateCString("Mask").data(),nullptr,masker->mode==MaskMode::Mask))
                            detail::runMask<MaskId::ChangeMaskMode>(ctx,masker->maskSrc.lock(),MaskMode::Mask);
                        if(ImGui::MenuItem(ngTranslateCString("Dodge").data(),nullptr,masker->mode==MaskMode::DodgeMask))
                            detail::runMask<MaskId::ChangeMaskMode>(ctx,masker->maskSrc.lock(),MaskMode::DodgeMask);
                        ImGui::EndMenu();
                    }
                    if(ImGui::MenuItem(ngTranslateCString("Delete").data())) {
                        const auto ctx=context();
                        detail::runMask<MaskId::RemoveMask>(ctx,
                            detail::maskAt(compat::ngRequireObject(node)->masks,i,174).maskSrc.lock());
                        ImGui::EndPopup(); ImGui::PopID(); ImGui::EndListBox(); ngEndCategory(); return;
                    }
                    ImGui::EndPopup();
                }
                const auto pattern=core::translation::ngTranslateRetained(masker->mode==MaskMode::Mask ?
                    compat::RetainedString::fromStaticLiteral("%s (Mask)") :
                    compat::RetainedString::fromStaticLiteral("%s (Dodge)"));
                const auto maskName=compat::RetainedString(
                    compat::ngRequireObject(masker->maskSrc.lock())->getName());
                const std::array args{core::translation::ngFormatArgument(maskName)};
                const auto labelText=detail::retainedText(core::translation::ngFormatTyped(pattern.view(),args));
                const compat::SourceCString label{compat::RetainedStringSlice(labelText)};
                compat::detail::ngSourceCStringPostconditions(labelText.view(),label.data());
                ImGui::Selectable(label.data());
                if(ImGui::BeginDragDropTarget()) {
                    if(auto payload=ImGui::AcceptDragDropPayload("_MASKITEM")) {
                        if(auto binding=ngMaskFromDragDropPayload(payload)) {
                            const auto maskIdx=node->getMaskIdx(binding->maskSrcUUID);
                            if(maskIdx>=0) {
                                auto oldMasks=detail::duplicateMasks(node->masks);
                                auto moving=detail::maskAt(oldMasks,static_cast<std::size_t>(maskIdx),196);
                                const auto prefix=oldMasks.slice(0,static_cast<std::size_t>(maskIdx));
                                const auto suffix=oldMasks.slice(static_cast<std::size_t>(maskIdx)+1,oldMasks.size());
                                auto without=detail::concatenateMasks(prefix,suffix);
                                auto index=i; if(static_cast<std::size_t>(maskIdx)<index && index>0) --index;
                                index=std::min(index,without.size());
                                const auto before=without.slice(0,index);
                                const auto after=without.slice(index,without.size());
                                auto newMasks=detail::concatenateMasks(before,moving,after);
                                if(!detail::sameMasks(oldMasks,newMasks))
                                    core::actionstack::incActionPush(actions::node::PartMaskListChangeAction::createSourceDeferred(
                                        [&] { return translated("Reorder Mask Source"); },
                                        [&] { return node; },
                                        [&] { return oldMasks; },
                                        [&] { return newMasks; }));
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("###MaskSettings");
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ngSetMaskDragDropPayload(*masker,ImGuiCond_Always);
                    const auto payloadName=compat::ngRequireObject(masker->maskSrc.lock())->getName();
                    ngText(compat::RetainedStringSlice(compat::RetainedString(payloadName)));
                    ImGui::EndDragDropSource();
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
        if(ImGui::BeginDragDropTarget()) {
            if(auto payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
                auto raw=*static_cast<Node* const*>(payload->Data);
                if(auto drawable=raw ? std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(raw->shared_from_this()) : nullptr) {
                    if(!nicxlive::core::ngResourceObjectEquals(drawable,node) && !node->isMaskedBy(drawable))
                        detail::runMask<MaskId::AddMask>(context(),drawable,MaskMode::Mask);
                }
            }
            ImGui::EndDragDropTarget();
        }
        bool propagate=node->propagateMeshGroup;
        if(ngCheckbox(ngTranslateCString("Propagate MeshGroup").data(),&propagate)) {
            node->propagateMeshGroup=propagate;
            std::ptrdiff_t offset=0;
            if(node->parentPtr()) {
                const auto parent=compat::ngRequireObject(node->parentPtr());
                const auto children=parent->children;
                const auto found=std::find_if(children.begin(),children.end(),[node](auto child) {
                    return nicxlive::core::ngResourceObjectEquals(child,node);
                });
                offset=found==children.end() ? -1 : std::distance(children.begin(),found);
            }
            if(node->parentPtr()) node->reparent(node->parentPtr(),offset);
        }
        ngTooltip(translated("Allow ascendant MeshGroup to deform children of Composite"));
        ImGui::Spacing(); ImGui::Spacing();
    }
    ngEndCategory();
}
template<class T> requires std::derived_from<T,Composite>
void NodeInspector<ModelEditSubMode::Deform,T>::run(std::shared_ptr<Parameter> param,Cursor cursor) {
    using namespace widgets; using core::translation::ngTranslateCString;
    const auto translated=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(text)));
    };
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return;
    [[maybe_unused]] const auto node=this->targets[0];
    this->updateDeform(param,cursor);
    if(ngBeginCategory(ngTranslateCString("Composite").data())) {
        ImGui::BeginGroup(); ImGui::Indent(16);
        if(ngBeginCategory(ngTranslateCString("Textures").data())) {
            ImGui::PushID(0); ngText(translated("Tint (Multiply)"));
            this->deformRGB([&]{return tintR.get(self);},[&]{return tintG.get(self);},
                [&]{return tintB.get(self);}); ImGui::PopID();
            ImGui::PushID(1); ngText(translated("Tint (Screen)"));
            this->deformRGB([&]{return screenTintR.get(self);},[&]{return screenTintG.get(self);},
                [&]{return screenTintB.get(self);}); ImGui::PopID();
            ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();
        }
        ngEndCategory(); ImGui::Unindent(); ImGui::EndGroup(); ngText(translated("Opacity"));
        this->template deform<DeformDescriptor<T,"opacity","opacity">>([&]{return opacity.get(self);},[](auto id,float* value) {
            return ngInspectorDeformSliderFloat(id,value,0,1);
        });
        ImGui::Spacing(); ImGui::Spacing();
    }
    ngEndCategory();
}
}
