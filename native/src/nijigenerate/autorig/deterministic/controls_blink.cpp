#include "controls_blink.hpp"
#include <algorithm>
#include <span>

namespace nijigenerate::autorig::deterministic::controls_detail {

contracts::Point2 blinkDisplacement(std::string_view kind, contracts::Point2 point,
    double blink, double expression, const Frame& eye, Profile& aperture,
    const compat::Slice<double>& flatClose, const compat::Slice<double>& smileClose,
    const compat::Slice<double>& expressionOffset, const compat::Slice<double>& upperContact,
    const compat::Slice<double>& lowerContact) {
    const auto sample = [&aperture, &point](const compat::Slice<double>& values) {
        return aperture.sample(std::span<const double>(values.data(), values.size()), point[0]);
    };
    const double t = (point[0] + eye.width / 2) / eye.width;
    const double chord = flatClose[0] * (1 - t) + flatClose[flatClose.size() - 1] * t;
    const double seam = expression > 0 ? sample(flatClose) +
        expression * (sample(smileClose) - sample(flatClose)) :
        sample(flatClose) - expression * sample(expressionOffset);
    const double top = sample(upperContact);
    const double bottom = sample(lowerContact);
    double delta;
    if (kind == "sclera") {
        delta = blink * (chord + (1 - blink) * (seam - chord) - point[1]);
    } else if (kind == "lower") {
        delta = blink * (seam - bottom);
    } else if (kind == "fold") {
        delta = blink * (seam - top);
    } else {
        const double upper = std::max(0., bottom - top);
        double band = point[1] - top;
        // Source clamp bounds are [0,max(0,bottom-top)]; they cannot be reversed.
        if (band < 0.) band = 0.;
        else if (band > upper) band = upper;
        delta = blink * (seam - top - band);
    }
    return {0., delta};
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
