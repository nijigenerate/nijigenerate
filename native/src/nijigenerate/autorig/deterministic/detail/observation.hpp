#pragma once
#include "../observation.hpp"
#include "../linear.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <cmath>
#include <limits>
#include <algorithm>
#include <string>
#include <string_view>
namespace nijigenerate::autorig::deterministic::observation::detail {
using namespace contracts;
template<std::size_t N> void require(bool ok, const char (&message)[N], std::size_t line) {
    if (!ok) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/observation.d"),line);
}
inline core::settings::SettingsJsonObject& object(Json& value) {
    return value.sourceObjectRef();
}
inline Json at(Json value,std::string_view key) {
    return value.sourceIndexValue(key);
}
inline Json pointJson(Point2 p) { return core::settings::SettingsJsonArray{Json(p[0]), Json(p[1])}; }
inline double square(double x) { return x * x; }
inline std::size_t index(double v) {
    // Literal source cast: truncate representable values. The source does not
    // define exceptional out-of-range/NaN bits; no private sentinel is supplied.
    return static_cast<std::size_t>(v);
}
inline void imageSize(std::size_t count, std::size_t w, std::size_t h, std::size_t line) {
    require(w > 0 && h > 0 && w <= std::numeric_limits<std::size_t>::max() / h / 4 && count == w * h * 4,
        "Invalid imported texture dimensions",line);
}
compat::Slice<double> alphaDistance(compat::Slice<bool>, std::size_t, std::size_t, bool, Context);
}
