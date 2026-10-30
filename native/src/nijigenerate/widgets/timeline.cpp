#include "timeline.hpp"
#include "timeline_internal.hpp"
#include "dummy.hpp"
#include "label.hpp"
#include <animation/animation.hpp>
#include <algorithm>
#include <limits>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets::timeline {

namespace {
void writeHover(float* pointer, float value) {
    if (!pointer) compat::ngFailNullAccess(true);
    *pointer = value;
}
void drawLine(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImU32 color, float thickness = 1) {
    compat::ngRequireObject(drawList)->AddLine(a, b, color, thickness);
}
void drawTriangle(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImVec2 c, ImU32 color, float thickness) {
    compat::ngRequireObject(drawList)->AddTriangle(a, b, c, color, thickness);
}
void drawTriangleFilled(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImVec2 c, ImU32 color) {
    compat::ngRequireObject(drawList)->AddTriangleFilled(a, b, c, color);
}
void drawQuad(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, ImU32 color, float thickness) {
    compat::ngRequireObject(drawList)->AddQuad(a, b, c, d, color, thickness);
}
void drawQuadFilled(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, ImU32 color) {
    compat::ngRequireObject(drawList)->AddQuadFilled(a, b, c, d, color);
}
void setStorageFloat(ImGuiStorage* storage, ImGuiID id, float value) {
    compat::ngRequireObject(storage)->SetFloat(id, value);
}
float getStorageFloat(ImGuiStorage* storage, ImGuiID id, float fallback) {
    return compat::ngRequireObject(storage)->GetFloat(id, fallback);
}
}

void ngAnimationLaneHeader(nicxlive::core::AnimationLane& lane, float& width, float& height) {
    {
        const auto idReceiver = lane.paramRef;
        ImGui::PushID(idReceiver.get());
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, .15f));
    if (ImGui::BeginChild("HEADER", ImVec2(width, height), ImGuiChildFlags_Borders)) {
        ngDummy(ImVec2(0, 8));
        ImGui::Indent();
        ngText(compat::RetainedStringSlice(compat::RetainedString(
            compat::ngRequireObject(compat::ngRequireObject(lane.paramRef)->targetParam)->name)));
        ImGui::SameLine(0, 0);
        ngDummy(ImVec2(-24, 0));
        ImGui::SameLine(0, 0);
        ngTextLabel(compat::ngRequireObject(lane.paramRef)->targetAxis == 0 ? "X" : "Y");
        ImGui::Unindent();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopID();
}

void ngHeaderResizer(float& width, bool side) {
    ImGui::PushID(side ? "LEFT_RESIZE" : "RIGHT_RESIZE");
    auto* window = ImGui::GetCurrentWindow();
    auto* drawList = ImGui::GetWindowDrawList();
    auto& io = ImGui::GetIO();
    auto* storage = ImGui::GetStateStorage();
    const auto resizingId = ImGui::GetID("IsResizing");
    const auto directionId = ImGui::GetID("ResizeDirection");
    float height = static_cast<float>(minTrackHeight);
    const bool isResizing = compat::ngRequireObject(storage)->GetBool(resizingId, false);
    const bool resizeDirection = storage->GetBool(directionId, false);
    const ImVec2 start(compat::ngRequireObject(window)->OuterRectClipped.Max.x,
        compat::ngRequireObject(window)->OuterRectClipped.Max.y);
    const auto mouse = ImGui::GetMousePos();
    const ImVec2 size(compat::ngRequireObject(window)->WorkRect.Min.x -
            compat::ngRequireObject(window)->WorkRect.Max.x,
        compat::ngRequireObject(window)->WorkRect.Min.y - compat::ngRequireObject(window)->WorkRect.Max.y);
    ImVec2 dragStart;
    ImVec2 dragEnd;
    bool hovered;
    if (side) {
        hovered = mouse.y > start.y + size.y - dragSize && mouse.y < start.y + size.y + dragSize &&
                  mouse.x > start.x && mouse.x < start.x + size.x;
        dragStart = ImVec2(start.x, start.y + height);
        dragEnd = ImVec2(start.x + width, start.y + height);
    } else {
        hovered = mouse.y > start.y && mouse.y < start.y + size.y &&
                  mouse.x > start.x + size.x - dragSize && mouse.x < start.x + size.x + dragSize;
        dragStart = ImVec2(start.x + width, start.y);
        dragEnd = ImVec2(start.x + width, start.y + height);
    }
    if (isResizing) {
        drawLine(drawList, dragStart, dragEnd, ImGui::GetColorU32(ImGuiCol_SeparatorActive, .5f), dragSize);
        if (!io.MouseDown[ImGuiMouseButton_Left]) storage->SetBool(resizingId, false);
        if (resizeDirection) {
            height = std::clamp(mouse.y - start.y, static_cast<float>(minTrackHeight),
                                std::numeric_limits<float>::max());
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        } else {
            width = std::clamp(mouse.x - start.x, static_cast<float>(minHeaderWidth),
                               static_cast<float>(maxHeaderWidth));
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    } else if (hovered) {
        drawLine(drawList, dragStart, dragEnd, ImGui::GetColorU32(ImGuiCol_SeparatorHovered, .5f), dragSize);
        ImGui::SetMouseCursor(side ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            storage->SetBool(resizingId, true);
            storage->SetBool(directionId, side);
        }
    } else {
        drawLine(drawList, dragStart, dragEnd, ImGui::GetColorU32(ImGuiCol_Separator, .5f), dragSize);
    }
    ImGui::PopID();
}

void ngBeginTimelinePlayhead(nicxlive::core::Animation& animation, float zoom) {
    auto* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return;
    auto* storage = ImGui::GetStateStorage();
    const float actualFrameSize = static_cast<float>(frameWidth * zoom);
    const float fullSize = actualFrameSize * (static_cast<float>(animation.length) - 1.0f);
    const auto start = ImGui::GetCursorScreenPos();
    setStorageFloat(storage, ImGui::GetID("PlayHead_StartX"), start.x);
    setStorageFloat(storage, ImGui::GetID("PlayHead_StartY"), start.y);
    setStorageFloat(storage, ImGui::GetID("PlayHead_Width"), fullSize);
    setStorageFloat(storage, ImGui::GetID("PlayHead_Height"), window->ClipRect.Min.y - window->ClipRect.Max.y);
}

void ngEndTimelinePlayhead(nicxlive::core::Animation& animation, float zoom, float frame) {
    (void)animation;
    auto* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return;
    auto* drawList = ImGui::GetWindowDrawList();
    auto* storage = ImGui::GetStateStorage();
    const float actualFrameSize = static_cast<float>(frameWidth * zoom);
    const float startX = getStorageFloat(storage, ImGui::GetID("PlayHead_StartX"), 0);
    const float startY = getStorageFloat(storage, ImGui::GetID("PlayHead_StartY"), 0);
    const ImVec2 start(startX, startY);
    const float width = getStorageFloat(storage, ImGui::GetID("PlayHead_Width"), 0);
    const float height = getStorageFloat(storage, ImGui::GetID("PlayHead_Height"), 0);
    const ImRect area(start, ImVec2(start.x + width, start.y + height));
    const auto lineColor = ImGui::GetColorU32(ImGuiCol_Separator);
    const auto triangleColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 0, 0, 1));
    ImGui::PushClipRect(area.Min, area.Max, true);
    const float offsetX = start.x + frame * actualFrameSize;
    drawLine(drawList, ImVec2(offsetX, start.y), ImVec2(offsetX, start.y + height), lineColor);
    drawTriangle(drawList, ImVec2(offsetX - 4, start.y), ImVec2(offsetX + 4, start.y),
                          ImVec2(offsetX, start.y + 4), lineColor, 4);
    drawTriangleFilled(drawList, ImVec2(offsetX - 4, start.y), ImVec2(offsetX + 4, start.y),
                                ImVec2(offsetX, start.y + 4), triangleColor);
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(start);
}

void ngTimelineLane(nicxlive::core::AnimationLane& lane, nicxlive::core::Animation& animation,
                    float zoom, std::int32_t index, float* hoveredFrame, float* hoveredValue) {
    auto* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return;
    const auto id = ImGui::GetID(&lane);
    const float actualFrameSize = static_cast<float>(frameWidth * zoom);
    const float fullSize = actualFrameSize * (static_cast<float>(animation.length) - 1.0f);
    auto* drawList = ImGui::GetWindowDrawList();
    [[maybe_unused]] auto& io = ImGui::GetIO();
    [[maybe_unused]] auto* storage = ImGui::GetStateStorage();
    const int height = minTrackHeight;
    const auto start = ImGui::GetCursorScreenPos();
    const ImRect area(start, ImVec2(start.x + fullSize, start.y + height));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::RenderFrame(area.Min, area.Max,
                        ImGui::GetColorU32(index % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt));
    ImGui::PopStyleVar();
    const auto lineColor = ImGui::GetColorU32(ImGuiCol_Separator);
    ImGui::PushClipRect(area.Min, area.Max, true);
    const auto mouse = ImGui::GetMousePos();
    const float mx = mouse.x - start.x;
    const float my = mouse.y - start.y;
    if (mx >= 0 && mx < fullSize && my >= 0 && my < height) {
        writeHover(hoveredFrame, mx / actualFrameSize);
        writeHover(hoveredValue, 1 - my / height);
    } else {
        writeHover(hoveredFrame, -1);
    }
    const auto frameCount = animation.length;
    for (std::int32_t i = 0; i < frameCount; ++i) {
        const float offsetX = start.x + actualFrameSize * static_cast<float>(i);
        drawLine(drawList, ImVec2(offsetX, start.y), ImVec2(offsetX, start.y + height), lineColor);
    }
    const auto getKeyframeX = [&](float value) { return start.x + actualFrameSize * value; };
    [[maybe_unused]] const auto getKeyframeY = [&](nicxlive::core::param::Parameter& parameter,
                                                  std::int32_t axis, float value) {
        return start.y + height * (1 - parameter.mapAxis(static_cast<std::uint32_t>(axis), value));
    };
    [[maybe_unused]] auto parameter = compat::ngRequireObject(lane.paramRef)->targetParam;
    [[maybe_unused]] auto axis = compat::ngRequireObject(lane.paramRef)->targetAxis;
    if (!lane.frames.empty()) {
        const auto iterationFrames = lane.frames;
        for (auto frame : iterationFrames) {
            const float x = getKeyframeX(static_cast<float>(frame.frame));
            const float y = static_cast<float>(start.y + height / 2.0);
            drawQuad(drawList, ImVec2(x, y - keyframeSize), ImVec2(x + keyframeSize, y),
                              ImVec2(x, y + keyframeSize), ImVec2(x - keyframeSize, y), 0xff000000, 2);
            drawQuadFilled(drawList, ImVec2(x, y - keyframeSize), ImVec2(x + keyframeSize, y),
                                    ImVec2(x, y + keyframeSize), ImVec2(x - keyframeSize, y), 0xff0000ff);
        }
    }
    ImGui::PopClipRect();
    ImGui::ItemAdd(area, id);
    ImGui::ItemSize(ImVec2(fullSize, static_cast<float>(height)));
}
} // namespace nijigenerate::widgets::timeline
