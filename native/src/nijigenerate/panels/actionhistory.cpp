/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors: Luna Nielsen
*/
#include "actionhistory.hpp"
#include "nijigenerate/core/actionstack.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/source_c_string.hpp"
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::panels::actionhistory {
namespace stack = core::actionstack;
namespace text = core::translation;

namespace {
compat::SourceCString actionCString(actions::ActionText value) {
    compat::SourceCString result(value);
    compat::detail::ngSourceCStringPostconditions(value.view(), result.data());
    return result;
}
}

ActionHistoryPanel::ActionHistoryPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("History"),
        text::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("History")), true) {
    flags |= ImGuiWindowFlags_NoScrollbar;
}

void ActionHistoryPanel::onUpdate() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    widgets::ngText(text::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Undo History")));
    ImGui::Separator();
    const auto available = ImGui::GetContentRegionAvail();

    if (ImGui::BeginChild("##ActionList", ImVec2(0, available.y - 30.0f))) {
        if (!stack::incActionHistory().empty()) {
            const auto history = stack::incActionHistory();
            for (std::size_t index = 0; index < history.size(); ++index) {
                auto action = history[index];
                ImGui::PushID(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index)));
                if (index == 0) {
                    ImGui::PushID("ASBEGIN");
                    const auto undoDescription = actionCString(compat::ngRequireObject(action)->describeUndo());
                    const auto signedIndex = std::bit_cast<std::ptrdiff_t>(stack::incActionIndex());
                    if (ImGui::Selectable(undoDescription.data(),
                            index <= static_cast<std::size_t>(signedIndex))) {
                        stack::incActionSetIndex(0);
                    }
                    ImGui::PopID();
                }
                const auto description = actionCString(compat::ngRequireObject(action)->describe());
                if (ImGui::Selectable(description.data(), index + 1 <= stack::incActionIndex())) {
                    stack::incActionSetIndex(index + 1);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();
    const auto clearLabel = text::ngTranslateCString("Clear History");
    if (widgets::ngButtonColored(clearLabel.data(), ImVec2(0, 0))) {
        stack::incActionClearHistory(stack::ActionStackClear::CurrentLevel);
    }
    ImGui::SameLine(0, 0);

    const auto pattern = text::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("%d of %d"));
    const auto historyLength = stack::incActionHistory().size();
    const auto undoHistoryLength = stack::incActionGetUndoHistoryLength();
    const std::array<text::TypedFormatArgument, 2> arguments{
        text::ngFormatArgument(historyLength), text::ngFormatArgument(undoHistoryLength)};
    auto formattedCount = text::ngFormatTyped(pattern.view(), arguments);
    compat::RetainedString count;
    try {
        count = compat::RetainedString(std::move(formattedCount));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const compat::RetainedStringSlice countSlice(count);
    const auto measured = widgets::ngMeasureString(countSlice);
    widgets::ngDummy(ImVec2(-measured.x, 1));
    ImGui::SameLine(0, 0);
    widgets::ngText(countSlice);
}

void ngInitializeActionHistoryPanelModule() {
    ngInitializePanelModule<ActionHistoryPanel>();
}

} // namespace nijigenerate::panels::actionhistory
