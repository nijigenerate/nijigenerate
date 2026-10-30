#pragma once

#include "contracts.hpp"
#include "controls_detail.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {

// Actual private source value type; its explicit zero defaults override Point2.init.
struct Frame {
    contracts::Point2 origin{0., 0.}, tangent{0., 0.}, normal{0., 0.};
    double width = 0;

    contracts::Point2 local(contracts::Point2 point);
};

Frame frame(compat::Slice<contracts::Point2> support);
Frame eyeFrame(compat::Slice<contracts::Point2> support);
compat::Slice<double> contactProfile(compat::Slice<contracts::Point2> support, Frame frame, bool lowerEdge);

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::controls_detail::Frame> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.controls.Frame)";
    inline static constexpr char unqualifiedArgument[] = "(Frame)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.controls.Frame";
    inline static constexpr char unqualifiedElement[] = "Frame";
};
}
