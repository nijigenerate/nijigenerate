/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.asyncderivedupdateoverlay;

import bindbc.imgui;
import nijigenerate.core.asyncderivedupdate;
import nijigenerate.core.input : WorldToViewport;
import nijigenerate.core.window : incUiAccentColor;
import nijilive : vec3;
import std.algorithm.comparison : max, min;
import std.math : isFinite;

private enum float AsyncDerivedUpdateOverlayOpacity = 0.5f;
private enum float AsyncDerivedUpdateFontScale = 0.82f;
private enum float AsyncDerivedUpdateHorizontalPadding = 4.0f;
private enum float AsyncDerivedUpdateVerticalPadding = 1.0f;

private bool hasDisplay(
    ref const AsyncDerivedUpdateSnapshot snapshot,
    AsyncDerivedUpdateDisplay display,
) {
    return (snapshot.display & cast(uint)display) != 0;
}

private string displayLabel(ref const AsyncDerivedUpdateSnapshot snapshot) {
    return snapshot.label.length ? snapshot.label : "Update";
}

private ImVec2 progressSize(ref const AsyncDerivedUpdateSnapshot snapshot) {
    auto label = displayLabel(snapshot);
    ImVec2 labelSize;
    ImFont_CalcTextSizeA(
        &labelSize,
        igGetFont(),
        igGetFontSize() * AsyncDerivedUpdateFontScale,
        float.max,
        0.0f,
        label.ptr,
        label.ptr + label.length);
    return ImVec2(
        labelSize.x + AsyncDerivedUpdateHorizontalPadding * 2.0f,
        labelSize.y + AsyncDerivedUpdateVerticalPadding * 2.0f);
}

private void drawProgress(
    ref const AsyncDerivedUpdateSnapshot snapshot,
    ImDrawList* drawList,
    ImVec2 screenPosition,
) {
    auto size = progressSize(snapshot);
    auto bottomRight = ImVec2(
        screenPosition.x + size.x,
        screenPosition.y + size.y);
    auto fraction = incAsyncDerivedUpdateProgress(snapshot);
    auto fillRight = screenPosition.x + size.x * fraction;
    auto rounding = min(4.0f, size.y * 0.25f);
    auto backgroundColor = igGetColorU32(
        ImVec4(0.46f, 0.46f, 0.46f, AsyncDerivedUpdateOverlayOpacity));
    auto fillColor = igGetColorU32(
        incUiAccentColor(AsyncDerivedUpdateOverlayOpacity));
    auto borderColor = igGetColorU32(
        ImVec4(0.10f, 0.10f, 0.10f, AsyncDerivedUpdateOverlayOpacity));
    auto textColor = igGetColorU32(
        ImVec4(1.0f, 1.0f, 1.0f, AsyncDerivedUpdateOverlayOpacity));

    ImDrawList_AddRectFilled(
        drawList, screenPosition, bottomRight, backgroundColor, rounding);
    if (fraction > 0.0f) {
        ImDrawList_AddRectFilled(
            drawList,
            screenPosition,
            ImVec2(fillRight, bottomRight.y),
            fillColor,
            min(rounding, (fillRight - screenPosition.x) * 0.5f));
    }
    ImDrawList_AddRect(
        drawList,
        screenPosition,
        bottomRight,
        borderColor,
        rounding,
        ImDrawFlags.RoundCornersAll,
        1.0f);

    ImVec2 labelSize;
    auto label = displayLabel(snapshot);
    auto font = igGetFont();
    auto fontSize = igGetFontSize() * AsyncDerivedUpdateFontScale;
    ImFont_CalcTextSizeA(
        &labelSize,
        font,
        fontSize,
        float.max,
        0.0f,
        label.ptr,
        label.ptr + label.length);
    ImDrawList_AddText(
        drawList,
        font,
        fontSize,
        ImVec2(
            screenPosition.x + (size.x - labelSize.x) * 0.5f,
            screenPosition.y + (size.y - labelSize.y) * 0.5f),
        textColor,
        label.ptr,
        label.ptr + label.length);
}

private ImVec2 worldToScreen(vec3 world, ImVec2 viewportOrigin) {
    auto viewportPoint = WorldToViewport(world.x, world.y);
    return ImVec2(
        viewportOrigin.x + viewportPoint.x,
        viewportOrigin.y + viewportPoint.y);
}

private void drawOutline(
    ref const AsyncDerivedUpdateSnapshot snapshot,
    ImDrawList* drawList,
    ImVec2 viewportOrigin,
) {
    auto color = igGetColorU32(
        ImVec4(0.62f, 0.62f, 0.62f, AsyncDerivedUpdateOverlayOpacity));
    foreach (i; 0 .. snapshot.visual.outlineWorld.length / 2) {
        auto start = worldToScreen(
            snapshot.visual.outlineWorld[i * 2], viewportOrigin);
        auto end = worldToScreen(
            snapshot.visual.outlineWorld[i * 2 + 1], viewportOrigin);
        if (!start.x.isFinite || !start.y.isFinite ||
            !end.x.isFinite || !end.y.isFinite) continue;
        ImDrawList_AddLine(drawList, start, end, color, 1.0f);
    }
}

/** Draw value-only snapshots; no producer or model object is accessed here. */
void drawAsyncDerivedUpdateViewportOverlay(
    ref const AsyncDerivedUpdateSnapshot[] snapshots,
    ImDrawList* drawList,
    ImVec2 viewportOrigin,
    ImRect viewportRect,
) {
    if (drawList is null || snapshots.length == 0) return;

    auto clipMin = ImVec2(
        min(viewportRect.Min.x, viewportRect.Max.x),
        min(viewportRect.Min.y, viewportRect.Max.y));
    auto clipMax = ImVec2(
        max(viewportRect.Min.x, viewportRect.Max.x),
        max(viewportRect.Min.y, viewportRect.Max.y));
    if (!clipMin.x.isFinite || !clipMin.y.isFinite ||
        !clipMax.x.isFinite || !clipMax.y.isFinite ||
        clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) return;

    ImDrawList_PushClipRect(drawList, clipMin, clipMax, true);
    foreach (ref const snapshot; snapshots) {
        if (!incAsyncDerivedUpdateShowsViewportProgress(snapshot)) continue;
        if (hasDisplay(snapshot, AsyncDerivedUpdateDisplay.ViewportOutline) &&
            snapshot.visual.outlineWorld.length > 0)
            drawOutline(snapshot, drawList, viewportOrigin);

        if (!hasDisplay(snapshot, AsyncDerivedUpdateDisplay.ViewportBadge) ||
            !snapshot.visual.hasAnchor) continue;
        auto screenPosition = worldToScreen(
            snapshot.visual.anchorWorld, viewportOrigin);
        screenPosition.x += 6.0f;
        screenPosition.y -= 6.0f;
        if (!screenPosition.x.isFinite || !screenPosition.y.isFinite) continue;
        drawProgress(snapshot, drawList, screenPosition);
    }
    ImDrawList_PopClipRect(drawList);
}
