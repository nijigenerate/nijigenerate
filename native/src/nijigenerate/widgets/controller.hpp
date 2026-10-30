#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <limits>
#include <string_view>
#include <imgui.h>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::widgets {

struct EditableAxisPoint {
    std::int32_t origIndex = 0;
    bool fixed = false;
    float value = std::numeric_limits<float>::quiet_NaN();
    float normValue = std::numeric_limits<float>::quiet_NaN();
};

namespace detail {
bool ngControllerImpl(compat::RetainedStringSlice id,
    std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
    ImVec2 size, bool forceSnap, compat::RetainedStringSlice grabParameter,
    float offsetX, float offsetY, float circleSize, float frameWidth);
bool ngControllerImpl(std::string_view id, std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
    ImVec2 size, bool forceSnap, std::string_view grabParameter,
    float offsetX, float offsetY, float circleSize, float frameWidth);
}

template<float offsetX = 24.0f, float offsetY = 12.0f, float circleSize = 6.0f, float frameWidth = 2.0f>
bool ngController(compat::RetainedStringSlice id,
                  std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
                  ImVec2 size, bool forceSnap = false,
                  compat::RetainedStringSlice grabParameter = compat::RetainedString::emptyLiteral()) {
    return detail::ngControllerImpl(id, parameter, size, forceSnap, grabParameter,
                                    offsetX, offsetY, circleSize, frameWidth);
}

template<float offsetX = 24.0f, float offsetY = 12.0f, float circleSize = 6.0f, float frameWidth = 2.0f>
bool ngController(std::string_view id, std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
                  ImVec2 size, bool forceSnap = false, std::string_view grabParameter = {}) {
    return detail::ngControllerImpl(id, parameter, size, forceSnap, grabParameter,
                                    offsetX, offsetY, circleSize, frameWidth);
}

void ngControllerAxisDemo(compat::RetainedStringSlice id,
                          std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
                          std::array<compat::Slice<EditableAxisPoint>, 2>& axisPoints, ImVec2 size);

// Explicit native compatibility views borrow storage for the whole operation.
void ngControllerAxisDemo(std::string_view id,
                          std::shared_ptr<nicxlive::core::param::Parameter>& parameter,
                          std::array<compat::Slice<EditableAxisPoint>, 2>& axisPoints, ImVec2 size);

void ngDrawLineDashed(ImDrawList* list, ImVec2 from, ImVec2 to, ImU32 color,
                      float thickness = 1.0f, int segments = 50, float lineScale = 1.0f);

} // namespace nijigenerate::widgets
namespace nijigenerate::widgets::controller {
using ::nijigenerate::widgets::EditableAxisPoint;
using ::nijigenerate::widgets::ngController;
using ::nijigenerate::widgets::ngControllerAxisDemo;
using ::nijigenerate::widgets::ngDrawLineDashed;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::EditableAxisPoint> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.controller.EditableAxisPoint)";
    inline static constexpr char unqualifiedArgument[] = "(EditableAxisPoint)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.controller.EditableAxisPoint";
    inline static constexpr char unqualifiedElement[] = "EditableAxisPoint";
};
} // namespace nicxlive::core::d_runtime
