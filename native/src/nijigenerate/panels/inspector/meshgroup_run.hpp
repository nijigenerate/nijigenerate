#pragma once
#include "meshgroup.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/inspector/apply_node_commands.hpp>
#include <nijigenerate/commands/inspector/apply_node_table.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/translation.hpp>
#include <compat/source_allocator.hpp>
namespace nijigenerate::panels::inspector::meshgroup {
template<ModelEditSubMode targetMode,class T>
    requires(targetMode==ModelEditSubMode::Layout && std::derived_from<T,nicxlive::core::nodes::MeshGroup>)
void NodeInspector<targetMode,T>::run() {
    auto owner=std::static_pointer_cast<NodeInspector>(this->retainSelf());
    if(this->targets.empty()) return;
    [[maybe_unused]] const auto node = this->targets[0];
    const auto category=core::translation::ngTranslateCString("MeshGroup");
    if(widgets::ngBeginCategory(category.data())) {
        ImGui::Spacing();
        const auto edit=[&]<bool translate>() {
            const auto state=[&] {if constexpr(translate) return translateChildren.get(owner); else return dynamic.get(owner);};
            if(this->template shared<Descriptor<T,translate>>(state,[&] {
                const auto label=core::translation::ngTranslateCString(
                    translate ? "Translate origins" : "Dynamic Deformation (slower)");
                const auto current=state();
                return widgets::ngCheckbox(label.data(),&current->value);
            })) {
                auto context=std::allocate_shared<commands::Context>(
                    nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
                context->inspectors(commands::Context::Inspectors{owner});
                const Selection<Node> nodes(this->targets);
                context->nodes(static_cast<commands::Context::Nodes>(nodes));
                const auto current=state();
                namespace source=commands::inspector::apply_node;
                using Inspector=NodeInspector<ModelEditSubMode::Layout,nicxlive::core::nodes::MeshGroup>;
                if constexpr(translate) {
                    using Access=source::InspectorAttributeAccess<Inspector,&Inspector::translateChildren>;
                    using Actual=typename source::DefApply<"MeshGroupTranslateChildren",Inspector,Access,"MeshGroup","bool">::Apply;
                    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands,
                        source::InspectorNodeApplyCommand::MeshGroupTranslateChildren,context,
                        {"InspectorNodeApplyCommand.MeshGroupTranslateChildren","InspectorNodeApplyCommand","MeshGroupTranslateChildrenCommand"},
                        current->value);
                } else {
                    using Access=source::InspectorAttributeAccess<Inspector,&Inspector::dynamic>;
                    using Actual=typename source::DefApply<"MeshGroupDynamic",Inspector,Access,"MeshGroup","bool">::Apply;
                    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands,
                        source::InspectorNodeApplyCommand::MeshGroupDynamic,context,
                        {"InspectorNodeApplyCommand.MeshGroupDynamic","InspectorNodeApplyCommand","MeshGroupDynamicCommand"},
                        current->value);
                }
            }
        };
        edit.template operator()<false>();
        widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Whether the MeshGroup should dynamically deform children,\nthis is an expensive operation and should not be overused."))));
        edit.template operator()<true>();
        widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Translate origin of child nodes for non-Drawable object."))));
        ImGui::Spacing();
    }
    widgets::ngEndCategory();
}
}
