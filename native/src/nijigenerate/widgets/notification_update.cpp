#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <imgui.h>
#include <string>
#include <string_view>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {

void NotificationPopup::onUpdate() {
    // The D receiver is a live stack root through callback reentrancy. Retain
    // its real control block if a callback releases the last external owner.
    // Stack-allocated native extensions have no shared control block to retain.
    const auto receiverLifetime = weak_from_this().lock();
    (void)receiverLifetime;
    if (items_.empty()) return;

    auto* io = &ImGui::GetIO();
    const ImVec2 basePosition{io->DisplaySize.x * 0.5f, 8.0f};
    float yOffset = 0.0f;
    const ImVec4 lightGreen{0.67f, 0.75f, 0.63f, 1.0f};
    {
        // Capture the descriptor and its length, not a deep list of entries.
        // Callbacks can mutate/replace items_ while this storage stays alive.
        const auto drawingItems = items_;
        for (std::size_t next = drawingItems.size(); next > 0; --next) {
            const auto index = next - 1;
            const auto item = drawingItems[index];
            if (item.closed) continue;
            constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
            ImGui::SetNextWindowPos(ImVec2{basePosition.x, basePosition.y + yOffset},
                ImGuiCond_Always, ImVec2{0.5f, 0.0f});
            ImGui::PushStyleColor(ImGuiCol_WindowBg, lightGreen);
            const auto label = [&] {
                try { return std::string("##NotificationPopup") + std::to_string(item.id); }
                catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            const compat::SourceCString labelz{std::string_view(label)};
            compat::detail::ngSourceCStringPostconditions(label, labelz.data());
            if (ImGui::Begin(labelz.data(), nullptr, flags)) {
                if (item.callback) {
                    item.callback(io);
                } else if (item.message) {
                    // Preserve the primary source's formatted Text call.
                    ImGui::Text(item.message->data());
                    ImGui::SameLine();
                    if (ngButtonColored("\xee\x97\x8d", ImVec2{20.0f, 20.0f})) {
                        // This is intentionally the current member descriptor.
                        // A callback shortening it must not redirect to old data.
                        if (index >= items_.size())
                            throw compat::ngStaticError<compat::ArrayIndexError>(index, items_.size(),
                                compat::RetainedString::fromStaticLiteral(
                                    "source/nijigenerate/widgets/notification.d"), 91);
                        items_.data()[index].closed = true;
                    }
                }
            }
            const auto size = ImGui::GetWindowSize();
            yOffset += size.y + 6.0f;
            ImGui::End();
            ImGui::PopStyleColor();
            // No scope guard: callback/label/bounds failures propagate before
            // the subsequent End/Pop/timer/cull, matching the source ordering.
        }
    }

    const float deltaTime = ImGui::GetIO().DeltaTime;
    compat::Slice<Item> kept;
    const auto cullItems = items_;
    for (std::size_t index = 0; index < cullItems.size(); ++index) {
        auto item = cullItems[index];
        if (item.closed) continue;
        if (!item.infinite) item.remaining -= deltaTime;
        if (item.infinite || item.remaining > 0.0f) kept.append(std::move(item));
    }
    items_ = std::move(kept);
}

} // namespace nijigenerate::widgets
