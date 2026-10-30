/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors:
    - Luna Nielsen
    - Asahi Lina
*/
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
#include <param/parameter.hpp>
#include <algorithm>
#include <cmath>
#if !defined(_MSC_VER)
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif

#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#pragma fp_contract(off)
#endif

namespace nijigenerate::widgets {
namespace {
using ParameterHandle = std::shared_ptr<nicxlive::core::param::Parameter>;
// Retain each evaluated receiver for its full expression, while reading the
// original ref slot again for the next expression after callbacks/rebinding.
class ParameterSlot {
    ParameterHandle& slot_;
    struct Receiver {
        ParameterHandle owner;
        auto* operator->() const { return compat::ngRequireObject(owner.get()); }
    };
public:
    explicit ParameterSlot(ParameterHandle& slot) : slot_(slot) {}
    Receiver operator->() const { return {slot_}; }
};
template<class Text> const char* endOf(const Text& text) {
    return text.data() ? text.data() + text.size() : text.data();
}
template<class Text> std::string_view viewOf(const Text& text) {
    return text.data() ? std::string_view(text.data(), text.size()) : std::string_view{};
}
float sourceClamp(float value, float lower, float upper) {
    const float greater = value < lower ? lower : value;
    return upper < greater ? upper : greater;
}
template<class Array> decltype(auto) sourceAt(Array&& array, std::size_t index, std::size_t sourceLine) {
    if (index >= array.size()) {
        throw compat::ngStaticError<compat::ArrayIndexError>(index, array.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/controller.d"), sourceLine);
    }
    return array[index];
}
float controllerDistance(const nicxlive::core::nodes::Vec2& a, const nicxlive::core::nodes::Vec2& b) {
    // inmath stores the vector difference and each squared float component,
    // then accumulates into D real and performs the real sqrt before narrowing.
    // LDC 1.41.0 selects binary64 real for the Windows MSVC target.
#if defined(_MSC_VER)
    using SourceReal = double;
    static_assert(std::numeric_limits<SourceReal>::digits == 53);
#else
    using SourceReal = boost::multiprecision::cpp_bin_float_double_extended;
    static_assert(std::numeric_limits<SourceReal>::digits == 64);
#endif
    volatile float x = a.x - b.x;
    volatile float y = a.y - b.y;
    volatile float xSquared = x * x;
    volatile float ySquared = y * y;
    SourceReal squared = 0;
    squared += SourceReal(static_cast<float>(xSquared));
    squared += SourceReal(static_cast<float>(ySquared));
#if defined(_MSC_VER)
    const SourceReal length = std::sqrt(squared);
    return static_cast<float>(length);
#else
    const SourceReal length = boost::multiprecision::sqrt(squared);
    return length.convert_to<float>();
#endif
}
template<class Text> bool controller(Text strId,
    ParameterHandle& parameter, ImVec2 size,
    bool forceSnap, Text grabParam,
    float oRectOffsetX, float oRectOffsetY, float circleSize, float frameWidth) {
    ParameterSlot param(parameter);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return false;

    ImGuiID id = ImGui::GetID(strId.data(), endOf(strId));

    ImVec2 avail;
    avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = avail.x-size.x;
    if (!param->isVec2) size.y = 32;
    else if (size.y <= 0) size.y = avail.y-size.y;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiStyle* style = &ctx->Style;
    [[maybe_unused]] ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiIO* io = &ImGui::GetIO();


    ImVec2 mPos;
    ImVec2 vPos;
    vPos = ImGui::GetCursorScreenPos();
    bool bModified = false;

    if (param->isVec2) {
        ImRect fRect = ImRect(
            vPos,
            ImVec2(vPos.x + size.x, vPos.y + size.y)
        );

        ImRect oRect = ImRect(
            ImVec2(vPos.x+oRectOffsetX, vPos.y+oRectOffsetY),
            ImVec2((vPos.x + size.x)-oRectOffsetX, (vPos.y + size.y)-oRectOffsetY)
        );

        ImGui::PushID(id);

            ImGui::RenderFrame(oRect.Min, oRect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));

            float sDeltaX = param->max.x-param->min.x;
            float sDeltaY = param->max.y-param->min.y;

            ImVec2 vSecurity = ImVec2(15, 15);
            ImRect frameBB = ImRect(ImVec2(oRect.Min.x - vSecurity.x, oRect.Min.y - vSecurity.y), ImVec2(oRect.Max.x + vSecurity.x, oRect.Max.y + vSecurity.y));

            bool shouldSnap = forceSnap || io->KeyShift;
            bool hovered;
            bool held;
            [[maybe_unused]] bool pressed = ImGui::ButtonBehavior(frameBB, ImGui::GetID("##Zone"), &hovered, &held);
            if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                held = true;
            }
            if ((viewOf(grabParam) == param->name) || (hovered && held)) {
                mPos = ImGui::GetMousePos();
                ImVec2 vCursorPos = ImVec2(mPos.x - oRect.Min.x, mPos.y - oRect.Min.y);

                param->value = nicxlive::core::nodes::Vec2(
                    sourceClamp(vCursorPos.x / (oRect.Max.x - oRect.Min.x) * sDeltaX + param->min.x, param->min.x, param->max.x),
                    sourceClamp(vCursorPos.y / (oRect.Max.y - oRect.Min.y) * -sDeltaY + param->max.y, param->min.y, param->max.y)
                );

                // Snap to closest point mode
                if (shouldSnap) {
                    // Ordinary source assignment evaluates its destination first;
                    // the genuine virtual getter may then rebind the ref slot.
                    const auto destinationReceiver = compat::ngRequireObject(parameter);
                    auto& destination = destinationReceiver->value;
                    const auto snapped = param->getClosestKeypointValue();
                    destination = snapped;
                }

                bModified = true;
            }

            [[maybe_unused]] float fXLimit = 10.0f / oRect.GetWidth();
            [[maybe_unused]] float fYLimit = 10.0f / oRect.GetHeight();
            float fScaleX;
            float fScaleY;
            ImVec2 vCursorPos;

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImS32 uDotColor = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImS32 uDotColorOff = ImGui::GetColorU32(ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
            ImS32 uLineColor = ImGui::GetColorU32(style->Colors[ImGuiCol_Text]);
            ImS32 uDotKeyColor = ImGui::GetColorU32(style->Colors[ImGuiCol_TextDisabled]);
            ImS32 uDotKeyPartial = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            ImS32 uDotKeyComplete = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

            // AXES LINES
            for (std::size_t xIdx = 0, end = param->axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(param->axisPoints[0], xIdx, 108);
                float xPos = (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x;

                ngDrawLineDashed(
                    drawList,
                    ImVec2(
                        xPos,
                        oRect.Min.y
                    ),
                    ImVec2(
                        xPos,
                        oRect.Max.y
                    ),
                    uDotKeyColor,
                    1.0f,
                    24,
                    1.2f
                );

            }

            for (std::size_t yIdx = 0, end = param->axisPoints[1].size(); yIdx < end; ++yIdx) {
                float yVal = 1 - sourceAt(param->axisPoints[1], yIdx, 130);
                float yPos = (oRect.Max.y - oRect.Min.y) * yVal + oRect.Min.y;

                ngDrawLineDashed(
                    drawList,
                    ImVec2(
                        oRect.Min.x,
                        yPos),
                    ImVec2(
                        oRect.Max.x,
                        yPos),
                    uDotKeyColor,
                    1.0f,
                    40,
                    1.2f
                );
            }

            // OUTSIDE FRAME
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Min.y), uLineColor, frameWidth);
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(oRect.Max.x, oRect.Max.y), uLineColor, frameWidth);
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Min.x, oRect.Max.y), uLineColor, frameWidth);
            drawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Max.y), uLineColor, frameWidth);

            // AXES POINTS
            for (std::size_t xIdx = 0, end = param->axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(param->axisPoints[0], xIdx, 158);
                for (std::size_t yIdx = 0, end = param->axisPoints[1].size(); yIdx < end; ++yIdx) {
                    float yVal = 1 - sourceAt(param->axisPoints[1], yIdx, 160);

                    vCursorPos = ImVec2(
                        (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x,
                        (oRect.Max.y - oRect.Min.y) * yVal + oRect.Min.y
                    );

                    drawList->AddCircleFilled(vCursorPos, circleSize, uDotKeyColor, 16);

                    bool isPartial = false;
                    bool isComplete = true;
                    const auto bindings = param->bindings;
                    for (auto binding : bindings) {
                        if (sourceAt(sourceAt(compat::ngRequireObject(binding)->getIsSet(), xIdx, 172), yIdx, 172)) {
                            isPartial = true;
                        } else {
                            isComplete = false;
                        }
                    }

                    if (isComplete && isPartial)
                        drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotKeyComplete, 16);
                    else if (isPartial)
                        drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotKeyPartial, 16);
                }
            }

            // OFFSET VALUE
            fScaleX = ((param->latestInternal.x) - param->min.x) / sDeltaX;
            fScaleY = 1 - ((param->latestInternal.y) - param->min.y) / sDeltaY;
            vCursorPos = ImVec2(
                (oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x,
                (oRect.Max.y - oRect.Min.y) * fScaleY + oRect.Min.y
            );

            drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotColorOff, 16);

            // PARAM VALUE
            fScaleX = (param->value.x - param->min.x) / sDeltaX;
            fScaleY = 1 - (param->value.y - param->min.y) / sDeltaY;
            vCursorPos = ImVec2(
                (oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x,
                (oRect.Max.y - oRect.Min.y) * fScaleY + oRect.Min.y
            );

            drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotColor, 16);

        ImGui::PopID();

        ImGui::ItemAdd(fRect, id);
        ImGui::ItemSize(size);
    } else {
        const float lineHeight = 16;

        ImRect fRect = ImRect(
            vPos,
            ImVec2(vPos.x + size.x, vPos.y + size.y)
        );

        ImRect oRect = ImRect(
            ImVec2(vPos.x+oRectOffsetX, vPos.y+oRectOffsetY),
            ImVec2((vPos.x + size.x)-oRectOffsetX, (vPos.y + size.y)-oRectOffsetY)
        );

        ImGui::PushID(id);

            ImGui::RenderFrame(oRect.Min, oRect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
            float sDeltaX = param->max.x-param->min.x;

            ImVec2 vSecurity = ImVec2(15, 15);
            ImRect frameBB = ImRect(ImVec2(oRect.Min.x - vSecurity.x, oRect.Min.y - vSecurity.y), ImVec2(oRect.Max.x + vSecurity.x, oRect.Max.y + vSecurity.y));

            bool shouldSnap = forceSnap || io->KeyShift;
            bool hovered;
            bool held;
            [[maybe_unused]] bool pressed = ImGui::ButtonBehavior(frameBB, ImGui::GetID("##Zone"), &hovered, &held);
            if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                held = true;
            }
            if ((viewOf(grabParam) == param->name) || (hovered && held)) {
                mPos = ImGui::GetMousePos();
                ImVec2 vCursorPos = ImVec2(mPos.x - oRect.Min.x, mPos.y - oRect.Min.y);

                param->value.x = sourceClamp(vCursorPos.x / (oRect.Max.x - oRect.Min.x) * sDeltaX + param->min.x, param->min.x, param->max.x);

                // Snap to closest point mode
                if (shouldSnap) {
                    nicxlive::core::nodes::Vec2 closestPoint = param->value;
                    float closestDist = std::numeric_limits<float>::infinity();
                    for (std::size_t xIdx = 0, end = param->axisPoints[0].size(); xIdx < end; ++xIdx) {
                        nicxlive::core::nodes::Vec2 pos = nicxlive::core::nodes::Vec2(
                            (param->max.x - param->min.x) * sourceAt(param->axisPoints[0], xIdx, 250) + param->min.x,
                            0
                        );

                        float dist = controllerDistance(param->value, pos);
                        if (dist < closestDist) {
                            closestDist = dist;
                            closestPoint = pos;
                        }
                    }

                    // clamp to closest point
                    param->value = closestPoint;
                }

                bModified = true;
            }

            float fYCenter = oRect.Min.y+(oRect.GetHeight()/2);
            float fYCenterLineLen1th = lineHeight/2;
            float fScaleX;
            [[maybe_unused]] float fScaleY;
            ImVec2 vCursorPos;

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImS32 uDotColor = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImS32 uDotColorOff = ImGui::GetColorU32(ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
            ImS32 uLineColor = ImGui::GetColorU32(style->Colors[ImGuiCol_Text]);
            ImS32 uDotKeyColor = ImGui::GetColorU32(style->Colors[ImGuiCol_TextDisabled]);
            ImS32 uDotKeyFilled = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

            // AXES LINES
            for (std::size_t xIdx = 0, end = param->axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(param->axisPoints[0], xIdx, 284);
                float xPos = (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x;

                drawList->AddLine(ImVec2(
                        xPos,
                        fYCenter-fYCenterLineLen1th-(fYCenterLineLen1th/4)
                    ),
                    ImVec2(
                        xPos,
                        fYCenter+fYCenterLineLen1th
                    ),
                    uLineColor,
                    frameWidth);

            }

            // REF LINE
            drawList->AddLine(ImVec2(oRect.Min.x, fYCenter), ImVec2(oRect.Max.x, fYCenter), uLineColor, frameWidth);

            // AXES POINTS
            for (std::size_t xIdx = 0, end = param->axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(param->axisPoints[0], xIdx, 308);

                vCursorPos = ImVec2(
                    (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x,
                    fYCenter
                );

                drawList->AddCircleFilled(vCursorPos, circleSize, uDotKeyColor, 16);
                const auto bindings = param->bindings;
                for (auto binding : bindings) {
                    if (sourceAt(sourceAt(compat::ngRequireObject(binding)->getIsSet(), xIdx, 317), 0, 317)) {
                        drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotKeyFilled, 16);
                        break;
                    }
                }
            }

            // OFFSET VALUE
            fScaleX = ((param->latestInternal.x) - param->min.x) / sDeltaX;
            vCursorPos = ImVec2(
                (oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x,
                fYCenter
            );

            drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotColorOff, 16);

            // PARAM VALUE
            fScaleX = (param->value.x - param->min.x) / sDeltaX;
            vCursorPos = ImVec2(
                (oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x,
                fYCenter
            );

            drawList->AddCircleFilled(vCursorPos, circleSize * 2 / 3, uDotColor, 16);

        ImGui::PopID();

        ImGui::ItemAdd(fRect, id);
        ImGui::ItemSize(size);
    }

    return bModified;
}

template<class Text> void axisDemo(Text strId,
    ParameterHandle& parameter,
    std::array<compat::Slice<EditableAxisPoint>, 2>& axisPoints, ImVec2 size) {
    ParameterSlot param(parameter);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return;

    ImGuiID id = ImGui::GetID(strId.data(), endOf(strId));

    ImVec2 avail;
    avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = avail.x-size.x;
    if (!param->isVec2) size.y = 32;
    else if (size.y <= 0) size.y = avail.y-size.y;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiStyle* style = &ctx->Style;
    [[maybe_unused]] ImGuiStorage* storage = ImGui::GetStateStorage();
    [[maybe_unused]] ImGuiIO* io = &ImGui::GetIO();


    ImVec2 mPos;
    ImVec2 vPos;
    vPos = ImGui::GetCursorScreenPos();

    if (param->isVec2) {
        float oRectOffsetX = 24;
        float oRectOffsetY = 12;
        ImRect fRect = ImRect(
            vPos,
            ImVec2(vPos.x + size.x, vPos.y + size.y)
        );

        ImRect oRect = ImRect(
            ImVec2(vPos.x+oRectOffsetX, vPos.y+oRectOffsetY),
            ImVec2((vPos.x + size.x)-oRectOffsetX, (vPos.y + size.y)-oRectOffsetY)
        );

        ImGui::PushID(id);

            ImGui::RenderFrame(oRect.Min, oRect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
            ImVec2 vCursorPos;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImS32 uLineColor = ImGui::GetColorU32(style->Colors[ImGuiCol_Text]);
            ImS32 uDotKeyColor = ImGui::GetColorU32(style->Colors[ImGuiCol_TextDisabled]);
            [[maybe_unused]] ImS32 uDotKeyFilled = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

            // AXES LINES
            for (std::size_t xIdx = 0, end = axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(axisPoints[0], xIdx, 401).normValue;
                float xPos = (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x;

                ngDrawLineDashed(
                    drawList,
                    ImVec2(
                        xPos,
                        oRect.Min.y
                    ),
                    ImVec2(
                        xPos,
                        oRect.Max.y
                    ),
                    uDotKeyColor,
                    1.0f,
                    24,
                    1.2f
                );
            }

            for (std::size_t yIdx = 0, end = axisPoints[1].size(); yIdx < end; ++yIdx) {
                float yVal = 1 - sourceAt(axisPoints[1], yIdx, 422).normValue;
                float yPos = (oRect.Max.y - oRect.Min.y) * yVal + oRect.Min.y;

                ngDrawLineDashed(
                    drawList,
                    ImVec2(
                        oRect.Min.x,
                        yPos),
                    ImVec2(
                        oRect.Max.x,
                        yPos),
                    uDotKeyColor,
                    1.0f,
                    40,
                    1.2f
                );
            }

            // OUTSIDE FRAME
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Min.y), uLineColor, 2.0f);
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(oRect.Max.x, oRect.Max.y), uLineColor, 2.0f);
            drawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Min.x, oRect.Max.y), uLineColor, 2.0f);
            drawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(oRect.Max.x, oRect.Max.y), uLineColor, 2.0f);

            // AXES POINTS
            for (std::size_t xIdx = 0, end = axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(axisPoints[0], xIdx, 450).normValue;
                for (std::size_t yIdx = 0, end = axisPoints[1].size(); yIdx < end; ++yIdx) {
                    float yVal = 1 - sourceAt(axisPoints[1], yIdx, 452).normValue;

                    vCursorPos = ImVec2(
                        (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x,
                        (oRect.Max.y - oRect.Min.y) * yVal + oRect.Min.y
                    );

                    drawList->AddCircleFilled(vCursorPos, 6.0f, uDotKeyColor, 16);
                }
            }
        ImGui::PopID();

        ImGui::ItemAdd(fRect, id);
        ImGui::ItemSize(size);
    } else {
        const float lineHeight = 16;

        float oRectOffsetX = 24;
        float oRectOffsetY = 12;
        ImRect fRect = ImRect(
            vPos,
            ImVec2(vPos.x + size.x, vPos.y + size.y)
        );

        ImRect oRect = ImRect(
            ImVec2(vPos.x+oRectOffsetX, vPos.y+oRectOffsetY),
            ImVec2((vPos.x + size.x)-oRectOffsetX, (vPos.y + size.y)-oRectOffsetY)
        );

        ImGui::PushID(id);

            ImGui::RenderFrame(oRect.Min, oRect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg));
            [[maybe_unused]] float sDeltaX = param->max.x-param->min.x;

            ImVec2 vSecurity = ImVec2(15, 15);
            ImRect frameBB = ImRect(ImVec2(oRect.Min.x - vSecurity.x, oRect.Min.y - vSecurity.y), ImVec2(oRect.Max.x + vSecurity.x, oRect.Max.y + vSecurity.y));
            float fYCenter = oRect.Min.y+(oRect.GetHeight()/2);
            float fYCenterLineLen1th = lineHeight/2;
            ImVec2 vCursorPos;

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImS32 uLineColor = ImGui::GetColorU32(style->Colors[ImGuiCol_Text]);
            ImS32 uDotKeyColor = ImGui::GetColorU32(style->Colors[ImGuiCol_TextDisabled]);
            [[maybe_unused]] ImS32 uDotKeyFilled = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

            // AXES LINES
            for (std::size_t xIdx = 0, end = axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(axisPoints[0], xIdx, 500).normValue;
                float xPos = (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x;

                drawList->AddLine(ImVec2(
                        xPos,
                        fYCenter-fYCenterLineLen1th-(fYCenterLineLen1th/4)
                    ),
                    ImVec2(
                        xPos,
                        fYCenter+fYCenterLineLen1th
                    ),
                    uLineColor,
                    2.0f);

            }

            // REF LINE
            drawList->AddLine(ImVec2(oRect.Min.x, fYCenter), ImVec2(oRect.Max.x, fYCenter), uLineColor, 2.0f);

            // AXES POINTS
            for (std::size_t xIdx = 0, end = axisPoints[0].size(); xIdx < end; ++xIdx) {
                float xVal = sourceAt(axisPoints[0], xIdx, 524).normValue;

                vCursorPos = ImVec2(
                    (oRect.Max.x - oRect.Min.x) * xVal + oRect.Min.x,
                    fYCenter
                );

                drawList->AddCircleFilled(vCursorPos, 6.0f, uDotKeyColor, 16);
            }

        ImGui::PopID();

        ImGui::ItemAdd(fRect, id);
        ImGui::ItemSize(size);
    }
}

} // namespace

bool detail::ngControllerImpl(compat::RetainedStringSlice id, ParameterHandle& parameter,
    ImVec2 size, bool snap, compat::RetainedStringSlice grab, float x, float y, float circle, float frame) {
    return controller(id, parameter, size, snap, grab, x, y, circle, frame);
}
bool detail::ngControllerImpl(std::string_view id, ParameterHandle& parameter,
    ImVec2 size, bool snap, std::string_view grab, float x, float y, float circle, float frame) {
    return controller(id, parameter, size, snap, grab, x, y, circle, frame);
}
void ngControllerAxisDemo(compat::RetainedStringSlice id, ParameterHandle& parameter,
    std::array<compat::Slice<EditableAxisPoint>, 2>& points, ImVec2 size) {
    axisDemo(id, parameter, points, size);
}
void ngControllerAxisDemo(std::string_view id, ParameterHandle& parameter,
    std::array<compat::Slice<EditableAxisPoint>, 2>& points, ImVec2 size) {
    axisDemo(id, parameter, points, size);
}
} // namespace nijigenerate::widgets

#if defined(_MSC_VER)
#pragma float_control(pop)
#endif
