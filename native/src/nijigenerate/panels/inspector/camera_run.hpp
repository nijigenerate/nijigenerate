#pragma once
#include "camera.hpp"
#include <nijigenerate/commands/inspector/apply_node_commands.hpp>
#include <nijigenerate/commands/inspector/apply_node_table.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/drag_float_fields.hpp>
#include <nijigenerate/core/translation.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::panels::inspector::camera {
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, ext::nodes::ExCamera>)
void NodeInspector<targetMode, T>::run() {
    auto owner = std::static_pointer_cast<NodeInspector>(this->retainSelf());
    if (this->targets.empty()) return;
    auto node = std::static_pointer_cast<ext::nodes::ExCamera>(this->targets[0]);
    const auto category = core::translation::ngTranslateCString("Camera");
    if (widgets::ngBeginCategory(category.data())) {
        widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Viewport"))));
        ImGui::Indent();
        ImGui::SetNextItemWidth(widgets::ngAvailableSpace().x);
        if (this->template shared<ViewportDescriptor<T>>([&] { return viewportOrigin.get(owner); }, [&] {
            const auto state = viewportOrigin.get(owner);
            return widgets::ngDragFloat2Fields("###VIEWPORT", state->value.x, state->value.y, 2);
        })) {
            auto context = std::allocate_shared<commands::Context>(
                nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
            context->inspectors(commands::Context::Inspectors{owner});
            const Selection<Node> nodes(this->targets);
            context->nodes(static_cast<commands::Context::Nodes>(nodes));
            const auto state = viewportOrigin.get(owner);
            namespace source = commands::inspector::apply_node;
            using Inspector = NodeInspector<ModelEditSubMode::Layout, ext::nodes::ExCamera>;
            using Access = source::InspectorAttributeAccess<Inspector, &Inspector::viewportOrigin>;
            using Actual = typename source::DefApply<"ViewportOrigin", Inspector, Access,
                "ExCamera", "Vector!(float, 2)">::Apply;
            // Preserve cmd's auto-ref to the genuine value field through its
            // real registry/scope/type checks, then applyArgs reads the value.
            (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands,
                source::InspectorNodeApplyCommand::ViewportOrigin, context,
                {"InspectorNodeApplyCommand.ViewportOrigin", "InspectorNodeApplyCommand", "ViewportOriginCommand"},
                state->value);
        }
        ext::nodes::incVerifyCameraSizeShowWarning(node);
        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::Spacing();
    }
    widgets::ngEndCategory();
}
}
