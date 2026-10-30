// Copyright 2026, nijigenerate Project. Distributed under the 2-Clause BSD License.
#include "input.hpp"
#include "dpi.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <math/mat3.hpp>
#include <math/inmath_float.hpp>
#include <runtime_state.hpp>
#include <cstddef>

namespace nijigenerate::core::input {
namespace {
using nicxlive::core::math::Mat3x3;
Mat3x3 scaling(float x, float y) {
    Mat3x3 result;
    result.m[0][0] = x;
    result.m[1][1] = y;
    return result;
}
Mat3x3 translation(float x, float y) {
    Mat3x3 result;
    result.m[0][2] = x;
    result.m[1][2] = y;
    // inmath's 3x3 translation(x, y, 0) writes the bottom-right element to zero.
    result.m[2][2] = 0;
    return result;
}
// The current inmath inverse has no singular-matrix identity fallback.
// Adapted from inmath 1.0.6 (MIT; input_inmath.LICENSE).
Mat3x3 inputInverse(const Mat3x3& value) {
    const auto& m = value.m;
    const float determinant = m[0][0] * m[1][1] * m[2][2] + m[0][1] * m[1][2] * m[2][0] +
        m[0][2] * m[1][0] * m[2][1] - m[0][2] * m[1][1] * m[2][0] -
        m[0][1] * m[1][0] * m[2][2] - m[0][0] * m[1][2] * m[2][1];
    const float reciprocal = 1.0f / determinant;
    Mat3x3 result;
    result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * reciprocal;
    result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * reciprocal;
    result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * reciprocal;
    result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * reciprocal;
    result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * reciprocal;
    result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * reciprocal;
    result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * reciprocal;
    result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * reciprocal;
    result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * reciprocal;
    return result;
}
nicxlive::core::math::Vec2 inputPoint(const Mat3x3& matrix, float x, float y) {
    const float point[]{x, y, 1};
    float output[2]{};
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            output[row] = nicxlive::core::math::detail::inmathAccumulate(output[row],
                nicxlive::core::math::detail::inmathProduct(matrix.m[row][column], point[column]));
    return {output[0], output[1]};
}
}

nicxlive::core::math::Vec2 WorldToViewport(float x, float y,
        std::shared_ptr<nicxlive::core::math::Camera> camera) {
    if (!camera) camera = nicxlive::core::inGetCameraHandle();
    const auto position = compat::ngRequireObject(camera)->position;
    const auto scale = compat::ngRequireObject(camera)->scale;
    const auto center = compat::ngRequireObject(camera)->getCenterOffset();
    const float uiScale = dpi::incGetUIScale();
    using namespace nicxlive::core::math;
    const auto transform = multiply(multiply(inputInverse(scaling(uiScale, uiScale)), scaling(scale.x, scale.y)),
        translation(position.x + center.x, position.y + center.y));
    return inputPoint(transform, x, y);
}

void ngInputSetViewportMouse(float x, float y) {
    const auto position = compat::ngRequireObject(nicxlive::core::inGetCameraHandle())->position;
    const auto scale = compat::ngRequireObject(nicxlive::core::inGetCameraHandle())->scale;
    const auto center = compat::ngRequireObject(nicxlive::core::inGetCameraHandle())->getCenterOffset();
    const float uiScale = dpi::incGetUIScale();
    using namespace nicxlive::core::math;
    const auto transform = multiply(multiply(translation(position.x + center.x, position.y + center.y),
        inputInverse(scaling(scale.x, scale.y))), scaling(uiScale, uiScale));
    detail::mpos = inputPoint(transform, x, y);
}

bool ngInputIsMouseDown(int index) {
    const auto* io = detail::io;
    const auto offset = static_cast<std::size_t>(index);
    constexpr std::size_t count = IM_ARRAYSIZE(io->MouseDown);
    if (offset >= count) throw compat::ngStaticError<compat::ArrayIndexError>(offset, count,
        compat::RetainedString::fromStaticLiteral("nijigenerate/core/input.d"), 79);
    return compat::ngRequireObject(io)->MouseDown[offset];
}
bool ngInputIsPopupRequested() {
    return !ngInputIsDragRequested() && ngInputIsMouseReleased(ImGuiMouseButton_Right);
}

bool ngShortcut(compat::RetainedStringSlice shortcut, bool repeat) {
    const auto& io = ImGui::GetIO();
    compat::Slice<compat::RetainedStringSlice> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= shortcut.size(); ++i) {
        if (i == shortcut.size() || shortcut.view()[i] == '+') {
            parts.append(shortcut.slice(start, i));
            start = i + 1;
        }
    }
    if (parts.empty()) return false;
    bool needCtrl = false, needAlt = false, needShift = false, needSuper = false;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const auto token = parts[i].view();
        if (token == detail::ngTokCtrl.view()) needCtrl = true;
        else if (token == detail::ngTokAlt.view()) needAlt = true;
        else if (token == detail::ngTokShift.view()) needShift = true;
        else if (token == detail::ngTokSuper.view()) needSuper = true;
        else return false;
    }
    if constexpr (platform::shortcutAppleLabels) {
        if (settings::incSettingsGet<bool>(compat::RetainedString::fromStaticLiteral("MacSwapCmdCtrl"), false))
            std::swap(needCtrl, needSuper);
    }
    if (io.KeyCtrl != needCtrl || io.KeyAlt != needAlt || io.KeyShift != needShift || io.KeySuper != needSuper)
        return false;
    const auto keyToken = parts[parts.size() - 1];
    const auto key = ngKeyScancode(keyToken.view());
    return key != ImGuiKey_None && ImGui::IsKeyPressed(key, repeat);
}
bool ngShortcut(std::string_view shortcut, bool repeat) {
    const auto retained = detail::allocateShortcutBytes([&] { return compat::RetainedString(shortcut); });
    return ngShortcut(compat::RetainedStringSlice{retained}, repeat);
}
} // namespace nijigenerate::core::input
