#pragma once
// Private implementation declarations; not exported by timeline.hpp.
#include <imgui_internal.h>
#include <compat/object_metadata.hpp>
#include <limits>

namespace nijigenerate::widgets::timeline::detail {
struct TimelinePlayheadData {
    ImVec2 start{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    ImRect renderArea{
        ImVec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()),
        ImVec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN())};
};
} // namespace nijigenerate::widgets::timeline::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::timeline::detail::TimelinePlayheadData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.timeline.TimelinePlayheadData)";
    inline static constexpr char unqualifiedArgument[] = "(TimelinePlayheadData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.timeline.TimelinePlayheadData";
    inline static constexpr char unqualifiedElement[] = "TimelinePlayheadData";
};
}
