#pragma once
#include <nijigenerate/core/asyncderivedupdate.hpp>
#include <imgui.h>
struct ImRect;

namespace nijigenerate::viewport::asyncderivedupdateoverlay {
void drawAsyncDerivedUpdateViewportOverlay(
    const compat::Slice<core::asyncderivedupdate::AsyncDerivedUpdateSnapshot>& snapshots,
    ImDrawList* drawList, ImVec2 viewportOrigin, ImRect viewportRect);
}
