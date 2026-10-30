#pragma once
#include "part.hpp"
#include "composite.hpp"
#include <nijigenerate/commands/node/welding_commands.hpp>
#include <nijigenerate/commands/node/welding.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/dragdrop_mask_payload.hpp>
#include <nijigenerate/widgets/drag_float_fields.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <cmath>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
namespace nijigenerate::panels::inspector::part {
namespace detail {
using MaskBinding = nicxlive::core::nodes::MaskBinding;
using MaskArray = nicxlive::core::common::SharedArray<MaskBinding>;

inline MaskArray allocateMaskArray(std::size_t length) {
    try {
        return MaskArray(length, MaskBinding{});
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

// The source concatenation allocates the complete destination before copying.
inline MaskArray concatenateMasks(MaskArray left, MaskArray right) {
    auto result = allocateMaskArray(left.size() + right.size());
    std::size_t destination = 0;
    for (const auto& value : left) result[destination++] = value;
    for (const auto& value : right) result[destination++] = value;
    return result;
}

inline MaskArray concatenateMasks(MaskArray left, const MaskBinding& middle, MaskArray right) {
    auto result = allocateMaskArray(left.size() + 1 + right.size());
    std::size_t destination = 0;
    for (const auto& value : left) result[destination++] = value;
    result[destination++] = middle;
    for (const auto& value : right) result[destination++] = value;
    return result;
}

template<std::size_t N> compat::RetainedStringSlice translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<class Array> decltype(auto) sourceAt(Array& array,std::size_t index,std::size_t line) {
    if(index>=array.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index,array.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/part.d"),line);
    return array[index];
}
template<commands::node::welding::NodeWeldingCommand id, class... Args>
void runWelding(std::shared_ptr<commands::Context> context, Args&&... args) {
    namespace source = commands::node::welding;
    using Id = source::NodeWeldingCommand;
    static_assert(id == Id::AddWelding || id == Id::RemoveWelding || id == Id::ChangeWeldingWeight);
    using Actual = std::conditional_t<id == Id::AddWelding, source::AddWeldingCommand,
        std::conditional_t<id == Id::RemoveWelding, source::RemoveWeldingCommand, source::ChangeWeldingWeightCommand>>;
    constexpr auto key = id == Id::AddWelding ? "NodeWeldingCommand.AddWelding" :
        id == Id::RemoveWelding ? "NodeWeldingCommand.RemoveWelding" : "NodeWeldingCommand.ChangeWeldingWeight";
    constexpr auto type = id == Id::AddWelding ? "AddWeldingCommand" :
        id == Id::RemoveWelding ? "RemoveWeldingCommand" : "ChangeWeldingWeightCommand";
    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands, id, context,
        {key, "NodeWeldingCommand", type}, std::forward<Args>(args)...);
}
template<commands::node::mask::NodeMaskCommand id, class... Args>
void runMask(std::shared_ptr<commands::Context> context, Args&&... args) {
    namespace source = commands::node::mask;
    using Id = source::NodeMaskCommand;
    static_assert(id == Id::AddMask || id == Id::RemoveMask || id == Id::ChangeMaskMode);
    using Actual = std::conditional_t<id == Id::AddMask, source::AddMaskCommand,
        std::conditional_t<id == Id::RemoveMask, source::RemoveMaskCommand, source::ChangeMaskModeCommand>>;
    constexpr auto key = id == Id::AddMask ? "NodeMaskCommand.AddMask" :
        id == Id::RemoveMask ? "NodeMaskCommand.RemoveMask" : "NodeMaskCommand.ChangeMaskMode";
    constexpr auto type = id == Id::AddMask ? "AddMaskCommand" :
        id == Id::RemoveMask ? "RemoveMaskCommand" : "ChangeMaskModeCommand";
    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands, id, context,
        {key, "NodeMaskCommand", type}, std::forward<Args>(args)...);
}
} // namespace detail
template<class T> requires(std::derived_from<T,Part> && !std::derived_from<T,Composite>)
void NodeInspector<ModelEditSubMode::Layout,T>::run() {
    using namespace widgets; using core::translation::ngTranslate; using core::translation::ngTranslateCString;
    using Id=commands::inspector::apply_node::InspectorNodeApplyCommand;
    using MaskId=commands::node::mask::NodeMaskCommand;
    using MaskMode=nicxlive::core::nodes::MaskingMode;
    using WeldingId=commands::node::welding::NodeWeldingCommand;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return; auto node=this->targets[0];
    auto context=[&] {
        auto result=std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        result->inspectors(commands::Context::Inspectors{self});
        result->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets)));
        return result;
    };
    if(ngBeginCategory(ngTranslateCString("Part").data())) {
        if(!compat::ngRequireObject(node)->nicxlive::core::nodes::Drawable::getMesh().isReady()) {
            ImGui::Spacing(); ImGui::TextColored(CategoryTextColor,ngTranslateCString("Cannot inspect an unmeshed part").data());
            ngEndCategory(); return;
        }
        ImGui::Spacing();
        const float width=static_cast<float>(std::round((ngAvailableSpace().x/3.0)-2.0));
        const ImVec2 elementSize(width,width);
        if(this->targets.size()==1) {
            incInspectorTextureSlot(node,TextureUsage::Albedo,detail::translated("Albedo"),elementSize); ImGui::SameLine(0,4);
            incInspectorTextureSlot(node,TextureUsage::Emissive,detail::translated("Emissive"),elementSize); ImGui::SameLine(0,4);
            incInspectorTextureSlot(node,TextureUsage::Bumpmap,detail::translated("Bumpmap"),elementSize);
            ImGui::Spacing(); ImGui::Spacing();
        }
        const auto edit=[&]<PropertyId prop>(auto& property,Id id,auto fn) {
            const auto state=[&]{return property.get(self);};
            if(this->template shared<Descriptor<T,prop>>(state,[&]{return fn(state);})) {
                const auto ctx=context();
                const auto current=compat::ngRequireObject(state());
                ngRunPartPropertyCommand(id,ctx,std::cref(current->value));
            }
        };
        ngText(detail::translated("Tint (Multiply)"));
        edit.template operator()<"tint">(tint,Id::PartTint,[](auto state){
            const auto current=compat::ngRequireObject(state());
            return ngColorEdit3Fields("###TINT",current->value.x,current->value.y,current->value.z);
        });
        ngText(detail::translated("Tint (Screen)"));
        edit.template operator()<"screenTint">(screenTint,Id::PartScreenTint,[](auto state){
            const auto current=compat::ngRequireObject(state());
            return ngColorEdit3Fields("###S_TINT",current->value.x,current->value.y,current->value.z);
        });
        ngText(detail::translated("Emission Strength"));
        const auto emission=[&]{return emissionStrength.get(self);};
        float percentage=compat::ngRequireObject(emission())->value*100;
        if(this->template shared<Descriptor<T,"emissionStrength">>(emission,[&]{
                return ImGui::DragFloat("###S_EMISSION",&percentage,0.1f,0,std::numeric_limits<float>::max(),"%.0f%%");})) {
            compat::ngRequireObject(emission())->value=static_cast<float>(static_cast<double>(percentage)*0.01);
            const auto ctx=context();
            const auto current=compat::ngRequireObject(emission());
            ngRunPartPropertyCommand(Id::PartEmissionStrength,ctx,std::cref(current->value));
        }
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ngText(detail::translated("Blending"));
        edit.template operator()<"blendingMode">(blendingMode,Id::PartBlendingMode,[&](auto state) {
            const auto previous=compat::ngRequireObject(state())->value;
            if(ImGui::BeginCombo("###Blending",ngTranslateCString(ngPartBlendName(compat::ngRequireObject(state())->value)).data(),ImGuiComboFlags_HeightLarge)) {
                const std::pair<BlendMode,const char*> choices[]{
                    {BlendMode::Normal,"Normal"},{BlendMode::Multiply,"Multiply"},{BlendMode::Screen,"Screen"},
                    {BlendMode::Overlay,"Overlay"},{BlendMode::Darken,"Darken"},{BlendMode::Lighten,"Lighten"},
                    {BlendMode::ColorDodge,"Color Dodge"},{BlendMode::LinearDodge,"Linear Dodge"},{BlendMode::AddGlow,"Add (Glow)"},
                    {BlendMode::ColorBurn,"Color Burn"},{BlendMode::HardLight,"Hard Light"},{BlendMode::SoftLight,"Soft Light"},
                    {BlendMode::Subtract,"Subtract"},{BlendMode::Difference,"Difference"},{BlendMode::Exclusion,"Exclusion"},
                    {BlendMode::Inverse,"Inverse"},{BlendMode::DestinationIn,"Destination In"},
                    {BlendMode::ClipToLower,"Clip to Lower"},{BlendMode::SliceFromLower,"Slice from Lower"}};
                for(auto [mode,label]:choices) {
                    if(ImGui::Selectable(ngTranslateCString(label).data(),compat::ngRequireObject(state())->value==mode))
                        compat::ngRequireObject(state())->value=mode;
                    if(mode==BlendMode::Inverse) ngTooltip(detail::translated("Inverts the color by a factor of the overlying color"));
                    if(mode==BlendMode::ClipToLower) ngTooltip(detail::translated("Special blending mode that causes (while respecting transparency) the part to be clipped to everything underneath"));
                    if(mode==BlendMode::SliceFromLower) ngTooltip(detail::translated("Special blending mode that causes (while respecting transparency) the part to be slice by everything underneath.\nBasically reverse Clip to Lower."));
                } ImGui::EndCombo();
            } return compat::ngRequireObject(state())->value!=previous;
        });
        ImGui::Spacing(); ngText(detail::translated("Opacity"));
        edit.template operator()<"opacity">(opacity,Id::PartOpacity,[](auto state){
            const auto current=compat::ngRequireObject(state());
            return ImGui::SliderFloat("###Opacity",&current->value,0,1,"%0.2f");
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Masks").data()); ImGui::Spacing();
        ngText(detail::translated("Threshold"));
        edit.template operator()<"maskAlphaThreshold">(maskAlphaThreshold,Id::PartMaskAlphaThreshold,[](auto state){
            const auto current=compat::ngRequireObject(state());
            return ImGui::SliderFloat("###Threshold",&current->value,0,1,"%.2f");
        });
        if(std::dynamic_pointer_cast<Projectable>(node)) {
            const auto label=ngTranslateCString("Resize automatically");
            auto resize=autoResizedMesh.get(self);
            if(ngCheckbox(label.data(),&compat::ngRequireObject(resize)->value)) {
                const auto ctx=context();
                const auto current=compat::ngRequireObject(autoResizedMesh.get(self));
                ngRunPartPropertyCommand(Id::PartAutoResizedMesh,ctx,
                    std::cref(current->value));
            }
            ngTooltip(detail::translated("Resize size automatically when child nodes are added or removed. Affect performance severly, not recommended."));
        }
        ImGui::Spacing();
        if(this->targets.size()==1) {
            ngText(detail::translated("Mask Sources"));
if(ImGui::BeginListBox("###MaskSources",ImVec2(0,128))) {
            if(compat::ngRequireObject(node)->masks.empty()) ngText(detail::translated("(Drag a Part or Mask Here)"));
            const auto count=compat::ngRequireObject(node)->masks.size();
            for(std::size_t i=0;i<count;++i) {
                auto maskBacking=compat::ngRequireObject(node)->masks;
                auto* masker=&detail::sourceAt(maskBacking,i,359);
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::BeginPopup("###MaskSettings")) {
                    if(ImGui::MenuItem(ngTranslateCString("Focus").data())) { project::incFocusCamera(masker->maskSrc.lock()); project::incSelectNode(masker->maskSrc.lock()); }
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
                            detail::sourceAt(compat::ngRequireObject(node)->masks,i,379).maskSrc.lock());
                        ImGui::EndPopup(); ImGui::PopID(); ImGui::EndListBox(); ngEndCategory(); return;
                    }
                    ImGui::EndPopup();
                }
                const auto pattern=core::translation::ngTranslateRetained(masker->mode==MaskMode::Mask ?
                    compat::RetainedString::fromStaticLiteral("%s (Mask)") :
                    compat::RetainedString::fromStaticLiteral("%s (Dodge)"));
                const std::array<core::translation::TypedFormatArgument,1> args{
                    core::translation::ngFormatArgument(compat::ngRequireObject(masker->maskSrc.lock())->getName())};
                const auto labelText=composite::detail::retainedText(
                    core::translation::ngFormatTyped(pattern.view(),args));
                const compat::SourceCString label(labelText);
                compat::detail::ngSourceCStringPostconditions(labelText.view(),label.data());
                ImGui::Selectable(label.data());
                if(ImGui::BeginDragDropTarget()) {
                    if(auto payload=ImGui::AcceptDragDropPayload("_MASKITEM")) {
                        if(auto binding=ngMaskFromDragDropPayload(payload)) {
                            const auto maskIdx=compat::ngRequireObject(node)->getMaskIdx(binding->maskSrcUUID);
                            if(maskIdx>=0) {
                                const auto currentMasks=compat::ngRequireObject(node)->masks;
                                auto oldMasks=[&] {
                                    try {
                                        return currentMasks.duplicate();
                                    } catch (const std::bad_alloc&) {
                                        throw compat::ngStaticError<compat::OutOfMemoryError>(
                                            compat::RetainedString::fromStaticLiteral(
                                                "core/internal/array/construction.d"), 394);
                                    } catch (const std::length_error&) {
                                        throw compat::ngStaticError<compat::OutOfMemoryError>(
                                            compat::RetainedString::fromStaticLiteral(
                                                "core/internal/array/construction.d"), 394);
                                    }
                                }();
                                auto moving=detail::sourceAt(oldMasks,static_cast<std::size_t>(maskIdx),401);
                                const auto prefix=oldMasks.slice(0,static_cast<std::size_t>(maskIdx));
                                const auto suffix=oldMasks.slice(static_cast<std::size_t>(maskIdx)+1,oldMasks.size());
                                auto without=detail::concatenateMasks(prefix,suffix);
                                auto index=i; if(static_cast<std::size_t>(maskIdx)<index && index>0) --index;
                                index=std::min(index,without.size());
                                const auto before=without.slice(0,index);
                                const auto after=without.slice(index,without.size());
                                auto newMasks=detail::concatenateMasks(before,moving,after);
                                if(!composite::detail::sameMasks(oldMasks,newMasks))
                                    core::actionstack::incActionPush(actions::node::PartMaskListChangeAction::createSourceDeferred(
                                        [&] { return detail::translated("Reorder Mask Source"); },
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
                    ngText(compat::RetainedStringSlice(compat::RetainedString(
                        compat::ngRequireObject(masker->maskSrc.lock())->getName())));
                    ImGui::EndDragDropSource();
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
        if(ImGui::BeginDragDropTarget()) {
            if(auto payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
                auto raw=*compat::ngRequireObject(static_cast<Node* const*>(payload->Data));
                if(auto drawable=raw ? std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(raw->shared_from_this()) : nullptr) {
                    if(!nicxlive::core::ngResourceObjectEquals(drawable,node) && !compat::ngRequireObject(node)->isMaskedBy(drawable)) {
                        const auto ctx=context();
                        detail::runMask<MaskId::AddMask>(ctx,drawable,MaskMode::Mask);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
            ImGui::Spacing(); ngText(detail::translated("Welding"));
            if(ImGui::BeginListBox("###Welding",ImVec2(0,128))) {
                if(compat::ngRequireObject(node)->masks.empty()) ngText(detail::translated("(Drag a Part Here)"));
                const auto count=compat::ngRequireObject(node)->weldedLinks.size();
                for(std::size_t i=0;i<count;++i) {
                    auto backing=compat::ngRequireObject(node)->weldedLinks;
                    auto* welded=&detail::sourceAt(backing,i,461);
                    ImGui::PushID(static_cast<int>(i));
                    if(ImGui::BeginPopup("###WeldedLink")) {
                        if(ImGui::MenuItem(ngTranslateCString("Focus").data())) {project::incFocusCamera(welded->target.lock());project::incSelectNode(welded->target.lock());}
                        if(ImGui::MenuItem(ngTranslateCString("Delete").data())) {
                            const auto ctx=context();
                            detail::runWelding<WeldingId::RemoveWelding>(ctx,
                                detail::sourceAt(compat::ngRequireObject(node)->weldedLinks,i,471).target.lock());
                            ImGui::EndPopup(); ImGui::PopID(); ImGui::EndListBox(); ngEndCategory(); return;
                        }
                        ImGui::EndPopup();
                    }
                    const compat::RetainedString targetText(
                        compat::ngRequireObject(welded->target.lock())->getName());
                    const compat::SourceCString targetName(targetText);
                    compat::detail::ngSourceCStringPostconditions(targetText.view(),targetName.data());
                    ImGui::Selectable(targetName.data(),false,ImGuiSelectableFlags_AllowOverlap,ImVec2(0,17));
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("###WeldedLink");
                    ImGui::SameLine(0,0);
                    const std::array<core::translation::TypedFormatArgument,1> childArgs{
                        core::translation::ngFormatArgument(compat::ngRequireObject(welded->target.lock())->getName())};
                    const auto childText=composite::detail::retainedText(
                        core::translation::ngFormatTyped("###%s",childArgs));
                    const compat::SourceCString child(childText);
                    compat::detail::ngSourceCStringPostconditions(childText.view(),child.data());
                    if(ImGui::BeginChild(child.data(),ImVec2(0,15),ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize)) {
                        ngDummy(ImVec2(-64,1)); ImGui::SameLine(0,0); float weight=welded->weight;
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,1)); ImGui::SetNextItemWidth(64);
                        if(ImGui::SliderFloat("###weight",&weight,0,1,"%.2f")) {
                            const auto ctx=context();
                            detail::runWelding<WeldingId::ChangeWeldingWeight>(ctx,welded->target.lock(),weight);
                        }
                        ImGui::PopStyleVar();
                    }
                    ImGui::EndChild(); ImGui::PopID();
                }
                ImGui::EndListBox();
            }
            if(ImGui::BeginDragDropTarget()) {
                if(const auto* payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
                    auto* raw=*compat::ngRequireObject(static_cast<Node* const*>(payload->Data));
                    if(auto target=raw ? std::dynamic_pointer_cast<Drawable>(raw->shared_from_this()) : nullptr) {
                        if(!nicxlive::core::ngResourceObjectEquals(target,node) && !compat::ngRequireObject(node)->isWeldedBy(target) && !target->getVertices().empty()) {
                            const auto ctx=context();
                            detail::runWelding<WeldingId::AddWelding>(ctx,target,0.5f);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::Spacing(); ImGui::Spacing();
        }
    }
    ngEndCategory();
}
template<class T> requires(std::derived_from<T,Part> && !std::derived_from<T,Composite>)
void NodeInspector<ModelEditSubMode::Deform,T>::run(std::shared_ptr<Parameter> parameter,Cursor cursor) {
    using namespace widgets; using core::translation::ngTranslate; using core::translation::ngTranslateCString;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return;
    [[maybe_unused]] const auto node=this->targets[0];
    this->updateDeform(parameter,cursor);
    if(ngBeginCategory(ngTranslateCString("Part").data())) {
        ImGui::BeginGroup(); ImGui::Indent(16);
        if(ngBeginCategory(ngTranslateCString("Textures").data())) {
            ImGui::PushID(0); ngText(detail::translated("Tint (Multiply)"));
            this->deformRGB([&]{return tintR.get(self);},[&]{return tintG.get(self);},
                [&]{return tintB.get(self);}); ImGui::PopID();
            ImGui::PushID(1); ngText(detail::translated("Tint (Screen)"));
            this->deformRGB([&]{return screenTintR.get(self);},[&]{return screenTintG.get(self);},
                [&]{return screenTintB.get(self);}); ImGui::PopID();
            ngText(detail::translated("Emission Strength"));
            float percentage=compat::ngRequireObject(emissionStrength.get(self))->value*100;
            if(ImGui::DragFloat("###S_EMISSION",&percentage,0.1f,0,std::numeric_limits<float>::max(),"%.0f%%")) {
                compat::ngRequireObject(emissionStrength.get(self))->value=static_cast<float>(static_cast<double>(percentage)*0.01);
                compat::ngRequireObject(emissionStrength.get(self))->apply();
            }
            ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();
        }
        ngEndCategory(); ImGui::Unindent(); ImGui::EndGroup();
        ngText(detail::translated("Opacity"));
        this->template deform<DeformDescriptor<T,"opacity","opacity">>([&]{return opacity.get(self);},[](auto id,float* value) {
            return ngInspectorDeformSliderFloat(id,value,0,1);
        });
        ImGui::Spacing(); ImGui::Spacing(); ngText(detail::translated("Threshold"));
        this->template deform<DeformDescriptor<T,"alphaThreshold","alphaThreshold">>([&]{return alphaThreshold.get(self);},[](auto id,float* value) {
            return ngInspectorDeformSliderFloat(id,value,0,1);
        });
    }
    ngEndCategory();
}
}
