#include "nijigenerate/widgets/texture.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/core/logo.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <texture.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::widgets {
namespace {
compat::RetainedStringSlice retainLabel(std::string_view text) {
    try {
        return compat::RetainedStringSlice(compat::RetainedString(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> T quantize(T value, T unit) {
    if (unit != 0) {
        const T scaled = value / unit;
        if (!std::isinf(scaled)) return std::rint(scaled) * unit;
    }
    return value;
}
template<class T> T clamp(T value, T low, T high) {
    return std::min(std::max(value, low), high);
}
struct GridBounds { ImVec2 min; ImVec2 max; ImDrawList* draw; };
GridBounds preview(const std::shared_ptr<nicxlive::core::Texture>& texture,
                   ImVec2 size, float gridSize, ImVec2 start, ImVec2 padding) {
    const float qx = quantize(size.x, gridSize / 2);
    const float qy = quantize(size.y, gridSize / 2);
    auto* draw = ImGui::GetWindowDrawList();
    const auto screen = ImGui::GetCursorScreenPos();
    const ImVec2 lo(screen.x - padding.x / 2, screen.y - padding.y / 2);
    const ImVec2 hi(lo.x + size.x - padding.x, lo.y + size.y - padding.y);
    if (texture) {
        if (texture->colorMode() == 0x1908) { // GL_RGBA
            const auto uv = [gridSize](float q) {
                // The D literal 0.5 selects a double quantize instantiation.
                return static_cast<float>(clamp(quantize(static_cast<double>(q / gridSize), 0.5),
                    1.0, static_cast<double>(std::numeric_limits<float>::max())));
            };
            const auto grid = core::ngGetGrid();
            const auto id = static_cast<ImTextureID>(compat::ngRequireObject(grid)->getTextureId());
            const auto uvX = uv(qx);
            const auto uvY = uv(qy);
            const auto rounding = ImGui::GetStyle().FrameRounding;
            compat::ngRequireObject(draw)->AddImageRounded(
                id, lo, hi, ImVec2(0, 0), ImVec2(uvX, uvY), 0xFFFFFFFF, rounding);
        } else {
            const auto rounding = ImGui::GetStyle().FrameRounding;
            compat::ngRequireObject(draw)->AddRectFilled(lo, hi, 0xFF000000, rounding);
        }
        const float paddedX = size.x - padding.x * 2;
        const float paddedY = size.y - padding.y * 2;
        const float widthScale = paddedX / static_cast<float>(texture->width());
        const float heightScale = paddedY / static_cast<float>(texture->height());
        const float scale = std::min(widthScale, heightScale);
        if (widthScale == heightScale) {
            const auto id = static_cast<ImTextureID>(texture->getTextureId());
            const auto rounding = ImGui::GetStyle().FrameRounding;
            compat::ngRequireObject(draw)->AddImageRounded(
                id, lo, hi, ImVec2(0, 0), ImVec2(1, 1), 0xFFFFFFFF, rounding);
        } else {
            ImGui::SetCursorPos(start);
            float x = 0, y = 0;
            const float width = texture->width() * scale;
            const float height = texture->height() * scale;
            if (widthScale > heightScale) x = (paddedX - width) / 2;
            else if (widthScale < heightScale) y = (paddedY - height) / 2;
            ImGui::SetCursorPos(ImVec2(start.x + x, start.y + y));
            ImGui::Image(static_cast<ImTextureID>(texture->getTextureId()), ImVec2(width, height));
        }
    }
    return {lo, hi, draw};
}
}
void ngTextureSlot(compat::RetainedStringSlice text, std::shared_ptr<nicxlive::core::Texture> texture,
                   ImVec2 size, float gridSize, ImGuiWindowFlags flags) {
    const char* begin = text.data();
    const char* end = begin ? begin + text.size() : begin;
    if (ImGui::BeginChildFrame(ImGui::GetID(begin, end), size,
            flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const auto start = ImGui::GetCursorPos();
        const float paddingX = ImGui::GetStyle().FramePadding.x;
        const float paddingY = ImGui::GetStyle().FramePadding.y;
        const ImVec2 padding(paddingX, paddingY);
        ImGui::BeginGroup();
        if (texture) preview(texture, size, gridSize, start, padding);
        ImGui::EndGroup();
        const auto color = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(color.x, color.y, color.z, .5f));
        ImGui::SetCursorPos(start);
        const auto measured = ngMeasureString(text);
        const auto labelId = ImGui::GetID("LABEL");
        const float labelWidth = clamp(measured.x + padding.x * 2, 8.f, size.x - padding.x * 2);
        const float labelHeight = measured.y + padding.y * 2;
        if (ImGui::BeginChildFrame(labelId, ImVec2(labelWidth, labelHeight),
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ngText(text);
        }
        ImGui::EndChildFrame();
        ImGui::PopStyleColor();
    }
    ImGui::EndChildFrame();
}
void ngTextureSlotUntitled(compat::RetainedStringSlice name, std::shared_ptr<nicxlive::core::Texture> texture,
                           ImVec2 size, float gridSize, ImGuiWindowFlags flags, bool selected) {
    const char* begin = name.data();
    const char* end = begin ? begin + name.size() : begin;
    if (ImGui::BeginChildFrame(ImGui::GetID(begin, end), size,
            flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const auto start = ImGui::GetCursorPos();
        const float paddingX = ImGui::GetStyle().FramePadding.x;
        const float paddingY = ImGui::GetStyle().FramePadding.y;
        const ImVec2 padding(paddingX, paddingY);
        ImGui::BeginGroup();
        const auto bounds = preview(texture, size, gridSize, start, padding);
        if (selected) {
            const auto color = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            const auto rounding = ImGui::GetStyle().FrameRounding;
            compat::ngRequireObject(bounds.draw)->AddRect(
                bounds.min, bounds.max, color, rounding, ImDrawFlags_None, 2.f);
        }
        ImGui::EndGroup();
    }
    ImGui::EndChildFrame();
}
void ngTextureSlot(std::string_view text, std::shared_ptr<nicxlive::core::Texture> texture,
                   ImVec2 size, float gridSize, ImGuiWindowFlags flags) {
    ngTextureSlot(retainLabel(text),
        std::move(texture), size, gridSize, flags);
}
void ngTextureSlotUntitled(std::string_view name, std::shared_ptr<nicxlive::core::Texture> texture,
                           ImVec2 size, float gridSize, ImGuiWindowFlags flags, bool selected) {
    ngTextureSlotUntitled(retainLabel(name),
        std::move(texture), size, gridSize, flags, selected);
}
}
