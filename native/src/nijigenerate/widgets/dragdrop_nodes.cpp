#include "nijigenerate/widgets/dragdrop.hpp"
#include "nijigenerate/widgets/texture.hpp"
#include "nijigenerate/widgets/label.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nodes/node.hpp>
#include <nodes/part.hpp>
#include <bit>
#include <cstdint>

namespace nijigenerate::widgets {
namespace {
constexpr std::int32_t increment(std::int32_t value) noexcept {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) + std::uint32_t{1});
}
}
void ngDragdropNodeList(DragdropNodeHandle node) {
    ImGui::PushID(std::bit_cast<std::int32_t>(compat::ngRequireObject(node)->getUUID()));
    if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) {
        ngTextureSlotUntitled(compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("ICON")),
            part->textures[0], ImVec2(44, 44), 24, ImGuiWindowFlags_NoInputs);
    } else {
        const compat::RetainedString name(compat::ngRequireObject(node)->getName());
        ngText(compat::RetainedStringSlice(name));
    }
    ImGui::PopID();
}
void ngDragdropNodeList(compat::Slice<DragdropNodeHandle> nodes) {
    std::int32_t i = 0;
    std::int32_t f = 0;
    for (auto node : nodes) {
        if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) {
            f = increment(f);
            const auto previous = i;
            i = increment(i);
            if (previous != 0 && i % 4 != 1) ImGui::SameLine(0, 4);
            ImGui::PushID(std::bit_cast<std::int32_t>(part->getUUID()));
            ngTextureSlotUntitled(compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("ICON")),
                part->textures[0], ImVec2(44, 44), 24, ImGuiWindowFlags_NoInputs);
            ImGui::PopID();
        }
    }
    if (static_cast<std::size_t>(i) < nodes.size()) {
        if (f > 0) ImGui::Dummy(ImVec2(0, 4));
        for (auto node : nodes) {
            if (std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node)) continue;
            const compat::RetainedString name(compat::ngRequireObject(node)->getName());
            ngText(compat::RetainedStringSlice(name));
        }
    }
}
}
