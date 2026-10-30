// Copyright 2026, nijigenerate Project. Distributed under the 2-Clause BSD License.
#include "asyncderivedupdateoverlay.hpp"
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/window_theme.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::viewport::asyncderivedupdateoverlay {
namespace {
using namespace core::asyncderivedupdate;
constexpr float AsyncDerivedUpdateOverlayOpacity = .5f;
constexpr float AsyncDerivedUpdateFontScale = .82f;
constexpr float AsyncDerivedUpdateHorizontalPadding = 4.0f;
constexpr float AsyncDerivedUpdateVerticalPadding = 1.0f;

bool hasDisplay(const AsyncDerivedUpdateSnapshot& snapshot, AsyncDerivedUpdateDisplay display) {
    return (snapshot.display & static_cast<std::uint32_t>(display)) != 0;
}
compat::RetainedString displayLabel(const AsyncDerivedUpdateSnapshot& snapshot) {
    return snapshot.label.empty() ? compat::RetainedString::fromStaticLiteral("Update") : snapshot.label;
}
ImVec2 progressSize(const AsyncDerivedUpdateSnapshot& snapshot) {
    const auto label = displayLabel(snapshot);
    const auto size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * AsyncDerivedUpdateFontScale,
        std::numeric_limits<float>::max(), 0, label.data(), label.data() + label.size());
    return {size.x + AsyncDerivedUpdateHorizontalPadding * 2, size.y + AsyncDerivedUpdateVerticalPadding * 2};
}
void drawProgress(const AsyncDerivedUpdateSnapshot& snapshot, ImDrawList* drawList, ImVec2 position) {
    const auto size = progressSize(snapshot);
    const ImVec2 bottomRight(position.x + size.x, position.y + size.y);
    const auto fraction = incAsyncDerivedUpdateProgress(snapshot);
    const auto fillRight = position.x + size.x * fraction;
    const auto rounding = std::min(4.0f, size.y * .25f);
    const auto background = ImGui::GetColorU32(ImVec4(.46f, .46f, .46f, AsyncDerivedUpdateOverlayOpacity));
    const auto fill = ImGui::GetColorU32(core::ngUiAccentColor(AsyncDerivedUpdateOverlayOpacity));
    const auto border = ImGui::GetColorU32(ImVec4(.10f, .10f, .10f, AsyncDerivedUpdateOverlayOpacity));
    const auto text = ImGui::GetColorU32(ImVec4(1, 1, 1, AsyncDerivedUpdateOverlayOpacity));
    drawList->AddRectFilled(position, bottomRight, background, rounding);
    if (fraction > 0)
        drawList->AddRectFilled(position, ImVec2(fillRight, bottomRight.y), fill,
            std::min(rounding, (fillRight - position.x) * .5f));
    drawList->AddRect(position, bottomRight, border, rounding, ImDrawFlags_RoundCornersAll, 1);
    const auto label = displayLabel(snapshot);
    auto* font = ImGui::GetFont();
    const auto fontSize = ImGui::GetFontSize() * AsyncDerivedUpdateFontScale;
    const auto labelSize = font->CalcTextSizeA(fontSize, std::numeric_limits<float>::max(), 0,
        label.data(), label.data() + label.size());
    drawList->AddText(font, fontSize,
        ImVec2(position.x + (size.x - labelSize.x) * .5f, position.y + (size.y - labelSize.y) * .5f),
        text, label.data(), label.data() + label.size());
}
ImVec2 worldToScreen(nicxlive::core::math::Vec3 world, ImVec2 origin) {
    const auto point = core::input::WorldToViewport(world.x, world.y);
    return {origin.x + point.x, origin.y + point.y};
}
void drawOutline(const AsyncDerivedUpdateSnapshot& snapshot, ImDrawList* drawList, ImVec2 origin) {
    const auto color = ImGui::GetColorU32(ImVec4(.62f, .62f, .62f, AsyncDerivedUpdateOverlayOpacity));
    const auto count = snapshot.visual.outlineWorld.size() / 2;
    const auto point = [&snapshot](std::size_t index, std::size_t sourceLine) {
        const auto outline = snapshot.visual.outlineWorld.view();
        if (index >= outline.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, outline.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/asyncderivedupdateoverlay.d"),
            sourceLine);
        return outline[index];
    };
    for (std::size_t i = 0; i < count; ++i) {
        const auto start = worldToScreen(point(i * 2, 127), origin);
        const auto end = worldToScreen(point(i * 2 + 1, 129), origin);
        if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(end.x) || !std::isfinite(end.y))
            continue;
        drawList->AddLine(start, end, color, 1);
    }
}
}

void drawAsyncDerivedUpdateViewportOverlay(const compat::Slice<AsyncDerivedUpdateSnapshot>& snapshots,
        ImDrawList* drawList, ImVec2 viewportOrigin, ImRect viewportRect) {
    if (!drawList || snapshots.empty()) return;
    const ImVec2 clipMin(std::min(viewportRect.Min.x, viewportRect.Max.x),
        std::min(viewportRect.Min.y, viewportRect.Max.y));
    const ImVec2 clipMax(std::max(viewportRect.Min.x, viewportRect.Max.x),
        std::max(viewportRect.Min.y, viewportRect.Max.y));
    if (!std::isfinite(clipMin.x) || !std::isfinite(clipMin.y) ||
        !std::isfinite(clipMax.x) || !std::isfinite(clipMax.y) ||
        clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) return;
    drawList->PushClipRect(clipMin, clipMax, true);
    const auto range = snapshots;
    for (const auto& snapshot : range) {
        if (!incAsyncDerivedUpdateShowsViewportProgress(snapshot)) continue;
        if (hasDisplay(snapshot, AsyncDerivedUpdateDisplay::ViewportOutline) &&
            snapshot.visual.outlineWorld.size() > 0) drawOutline(snapshot, drawList, viewportOrigin);
        if (!hasDisplay(snapshot, AsyncDerivedUpdateDisplay::ViewportBadge) || !snapshot.visual.hasAnchor) continue;
        auto position = worldToScreen(snapshot.visual.anchorWorld, viewportOrigin);
        position.x += 6;
        position.y -= 6;
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) continue;
        drawProgress(snapshot, drawList, position);
    }
    drawList->PopClipRect();
}
} // namespace nijigenerate::viewport::asyncderivedupdateoverlay
