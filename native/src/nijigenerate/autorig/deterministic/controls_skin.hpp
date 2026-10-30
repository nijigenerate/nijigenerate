#pragma once

#include "controls_frame.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

struct SkinProfile {
    Frame frame;
    compat::Slice<double> y, left, right;
    double temple = 0, eye = 0, mouth = 0;
    contracts::Point2 eyeSpan, mouthSpan;

    double midline(double level);
};

contracts::Point3 barycentric(contracts::Point2 point, contracts::Point2 first,
    contracts::Point2 second, contracts::Point2 third);
contracts::Point2 gridSample(compat::Slice<contracts::Point2> values,
    compat::Slice<double> xs, compat::Slice<double> ys, contracts::Point2 point);

compat::Slice<bool> cheekSupport(compat::Slice<contracts::Point2> world,
    core::settings::SettingsJsonValue triangles, SkinProfile profile, int side);

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::controls_detail::SkinProfile> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.controls.SkinProfile)";
    inline static constexpr char unqualifiedArgument[] = "(SkinProfile)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.controls.SkinProfile";
    inline static constexpr char unqualifiedElement[] = "SkinProfile";
};
}
