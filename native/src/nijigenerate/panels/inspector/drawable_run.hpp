#pragma once
#include "drawable.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/inspector/apply_node_commands.hpp>
#include <nijigenerate/commands/inspector/apply_node_table.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/core/translation.hpp>
#include <compat/source_allocator.hpp>
namespace nijigenerate::panels::inspector::drawable {
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, nicxlive::core::nodes::Drawable> &&
        !std::derived_from<T, nicxlive::core::nodes::Part> && !std::derived_from<T, nicxlive::core::nodes::MeshGroup>)
void NodeInspector<targetMode,T>::run() {
    auto owner = std::static_pointer_cast<NodeInspector>(this->retainSelf());
    if (this->targets.empty()) return;
    [[maybe_unused]] const auto node = this->targets[0];
    const auto category = core::translation::ngTranslateCString("Drawable");
    if (widgets::ngBeginCategory(category.data())) {
        const auto available = widgets::ngAvailableSpace();
        ImGui::BeginGroup();
        const auto label = core::translation::ngTranslateCString("Texture Offset");
        ImGui::TextColored(CategoryTextColor, label.data());
        ImGui::PushItemWidth((available.x-4.f)/2.f);
        const auto edit = [&]<bool y>() {
            const auto state = [&] { if constexpr (y) return offsetY.get(owner); else return offsetX.get(owner); };
            ImGui::PushID(y ? 43 : 42);
            if (this->template shared<OffsetDescriptor<T,y>>(state,[&] {
                const auto current = state();
                const compat::RetainedStringSlice id(y
                    ? compat::RetainedString::fromStaticLiteral("offset_y")
                    : compat::RetainedString::fromStaticLiteral("offset_x"));
                const compat::RetainedStringSlice format(compat::RetainedString::fromStaticLiteral("%.2f"));
                return widgets::ngDragFloat(id, &current->value, 1,
                    -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), format,
                    ImGuiSliderFlags_NoRoundToFormat);
            })) {
                auto context = std::allocate_shared<commands::Context>(
                    nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
                context->inspectors(commands::Context::Inspectors{owner});
                const Selection<Node> nodes(this->targets);
                context->nodes(static_cast<commands::Context::Nodes>(nodes));
                // cmd's auto-ref argument names this actual State field, read
                // by applyArgs after the real command-scope/type checks.
                const auto current = state();
                namespace source = commands::inspector::apply_node;
                using Inspector = NodeInspector<ModelEditSubMode::Layout, nicxlive::core::nodes::Drawable>;
                using Id = source::InspectorNodeApplyCommand;
                if constexpr (y) {
                    using Access = source::InspectorAttributeAccess<Inspector, &Inspector::offsetY>;
                    using Actual = typename source::DefApply<"OffsetY", Inspector, Access, "Drawable", "float">::Apply;
                    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands, Id::OffsetY, context,
                        {"InspectorNodeApplyCommand.OffsetY", "InspectorNodeApplyCommand", "OffsetYCommand"},
                        current->value);
                } else {
                    using Access = source::InspectorAttributeAccess<Inspector, &Inspector::offsetX>;
                    using Actual = typename source::DefApply<"OffsetX", Inspector, Access, "Drawable", "float">::Apply;
                    (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands, Id::OffsetX, context,
                        {"InspectorNodeApplyCommand.OffsetX", "InspectorNodeApplyCommand", "OffsetXCommand"},
                        current->value);
                }
            }
            ImGui::PopID();
        };
        edit.template operator()<false>(); ImGui::SameLine(0,4); edit.template operator()<true>();
        ImGui::PopItemWidth(); ImGui::EndGroup();
    }
    widgets::ngEndCategory();
}
}
