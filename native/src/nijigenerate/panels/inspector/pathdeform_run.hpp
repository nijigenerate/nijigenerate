#pragma once
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
namespace nijigenerate::panels::inspector::pathdeform {
template<class T> requires std::derived_from<T,PathDeformer>
void NodeInspector<ModelEditSubMode::Layout,T>::run() {
    using namespace widgets;using core::translation::ngTranslateCString;
    using Id=commands::inspector::apply_node::InspectorNodeApplyCommand;
    if(this->targets.empty())return;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    auto node=compat::ngArrayAt(this->targets,0);
    const auto dispatch=[&](Id id,auto&& property) {
        auto context=std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        context->inspectors(commands::Context::Inspectors{self});
        context->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets)));
        const auto state=property();
        ngRunPathPropertyCommand(id,std::move(context),std::cref(state->value));
    };
    if(ngBeginCategory(ngTranslateCString("PathDeformer").data())) {
        ImGui::Spacing();ImGui::Spacing();
        const auto dynamicLabel=ngTranslateCString("Dynamic mode(slow)");
        auto dynamic=[&] {return this->dynamic.get(self);};
        if(ngCheckbox(dynamicLabel.data(),&dynamic()->value))dispatch(Id::PathDeformDynamic,dynamic);
        const auto physicsLabel=ngTranslateCString("Auto-Physics");
        auto physics=[&] {return this->physicsEnabled.get(self);};
        if(ngCheckbox(physicsLabel.data(),&physics()->value))dispatch(Id::PathDeformPhysicsEnabled,physics);
        ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Enabled / Disabled physics driver for vertices. If enabled, vertices are moved along with specified physics engine."))));
        auto curve=[&] {return this->curveType.get(self);};
        auto text=curve()->value==CurveType::Bezier?"Bezier":curve()->value==CurveType::Spline?"Spline":"Invalid";
        if(ImGui::BeginCombo("###PhysType",ngTranslateCString(text).data())) {
            bool changed=false;
            const auto bezierLabel=ngTranslateCString("Bezier");
            if(ImGui::Selectable(bezierLabel.data(),curve()->value==CurveType::Bezier)) {
                curve()->value=CurveType::Bezier;changed=true;
            }
            const auto splineLabel=ngTranslateCString("Spline");
            if(ImGui::Selectable(splineLabel.data(),curve()->value==CurveType::Spline)) {
                curve()->value=CurveType::Spline;changed=true;
            }
            if(changed)dispatch(Id::PathDeformCurveType,curve);ImGui::EndCombo();
        }
        ImGui::Spacing();
        if(physics()->value) {
            using Pendulum=nicxlive::core::nodes::ConnectedPendulumDriver;
            using Spring=nicxlive::core::nodes::ConnectedSpringPendulumDriver;
            auto impl=[&]() -> std::shared_ptr<nicxlive::core::nodes::ConnectedPhysicsDriver> {
                const auto adapter=std::dynamic_pointer_cast<PathDeformer::ConnectedDriverAdapter>(
                    compat::ngRequireObject(node)->getDriverSource());
                return adapter && adapter->impl()
                    ? std::shared_ptr<nicxlive::core::nodes::ConnectedPhysicsDriver>(adapter,adapter->impl()) : nullptr;
            };
            ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Physics Type"))));
            auto type=std::dynamic_pointer_cast<Pendulum>(impl())?"Pendulum":
                std::dynamic_pointer_cast<Spring>(impl())?"SpringPendulum":"None";
            if(ImGui::BeginCombo("###PhysType",ngTranslateCString(type).data())) {
                const auto pendulumLabel=ngTranslateCString("Pendulum");
                if(ImGui::Selectable(pendulumLabel.data(),std::dynamic_pointer_cast<Pendulum>(impl())!=nullptr)) {
                    if(!std::dynamic_pointer_cast<Pendulum>(impl())) {
                        std::unique_ptr<Pendulum> driver(new (std::nothrow) Pendulum(node));
                        if(!driver) throw compat::ngStaticError<compat::OutOfMemoryError>(
                            std::source_location::current());
                        node->setDriverSource(std::allocate_shared<PathDeformer::ConnectedDriverAdapter>(
                            nicxlive::core::d_runtime::SourceAllocator<PathDeformer::ConnectedDriverAdapter>{},
                            std::move(driver),node.get()));
                    }
                    this->capture(Selection<Node>(this->targets));
                }
                const auto springLabel=ngTranslateCString("SpringPendulum");
                if(ImGui::Selectable(springLabel.data(),std::dynamic_pointer_cast<Spring>(impl())!=nullptr)) {
                    if(!std::dynamic_pointer_cast<Spring>(impl())) {
                        std::unique_ptr<Spring> driver(new (std::nothrow) Spring(node));
                        if(!driver) throw compat::ngStaticError<compat::OutOfMemoryError>(
                            std::source_location::current());
                        node->setDriverSource(std::allocate_shared<PathDeformer::ConnectedDriverAdapter>(
                            nicxlive::core::d_runtime::SourceAllocator<PathDeformer::ConnectedDriverAdapter>{},
                            std::move(driver),node.get()));
                    }
                    this->capture(Selection<Node>(this->targets));
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();ImGui::PushID("PhysicsDriver");
            if(const auto driver=std::dynamic_pointer_cast<Pendulum>(impl())) {
                const float maximum=std::numeric_limits<float>::max();
                const auto control=[&]<detail::Attribute A>(int id,const char* title,const char* tooltip,
                    const char* label,auto& property,Id command,float low,float high,bool spacing) {
                    ImGui::PushID(id);
                    ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
                        compat::RetainedString::fromStaticView(title))));
                    // These private control keys are static literals at every call.
                    if(tooltip)ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
                        compat::RetainedString::fromStaticView(tooltip))));
                    auto state=[&] {return property.get(self);};
                    if(this->template shared<detail::Descriptor<T,A>>(state,[&] {
                        return ngDragFloat(compat::RetainedStringSlice(compat::RetainedString::fromStaticView(label)),
                            &state()->value,.01f,low,high,
                            compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("%.2f")),
                            ImGuiSliderFlags_NoRoundToFormat);
                    }))dispatch(command,state);
                    if(spacing){ImGui::Spacing();ImGui::Spacing();}ImGui::PopID();
                };
                control.template operator()<detail::Attribute::Gravity>(0,"Gravity scale",nullptr,"gravity",
                    this->gravity,Id::PathDeformGravity,-maximum,maximum,true);
                control.template operator()<detail::Attribute::RestoreConstant>(1,"Restore force",
                    "Force to restore for original position. If this force is weaker than the gravity, pendulum cannot restore to original position.",
                    "restoreConstant",this->restoreConstant,Id::PathDeformRestoreConstant,.01f,maximum,true);
                control.template operator()<detail::Attribute::Damping>(2,"Damping",nullptr,"damping",
                    this->damping,Id::PathDeformDamping,0,5,false);
                control.template operator()<detail::Attribute::InputScale>(3,"Input scale",
                    "Input force is multiplied by this factor. This should be specified when original position moved too much.",
                    "inputScale",this->inputScale,Id::PathDeformInputScale,0,maximum,true);
                control.template operator()<detail::Attribute::PropagateScale>(4,"Propagate scale",
                    "Specify the degree to convey movement of previous pendulum to next one.","propagateScale",
                    this->propagateScale,Id::PathDeformPropagateScale,0,maximum,false);
                ImGui::Spacing();ImGui::Spacing();
            } else if(const auto springPendulum=std::dynamic_pointer_cast<Spring>(impl())) {
                // The source retains this driver but intentionally draws no controls.
            }
            ImGui::PopID();
        }
    }
    ngEndCategory();this->drawDepthBoneSources(node);
}
}
