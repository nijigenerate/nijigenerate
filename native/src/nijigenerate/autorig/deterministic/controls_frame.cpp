#include "controls_frame.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>

namespace nijigenerate::autorig::deterministic::controls_detail {
namespace {

double supportPercentile(compat::Slice<double> values, double fraction) {
    // Consume the existing actual percentile/sort implementation without cloning it.
    return percentile(std::span<const double>(values.data(), values.size()), fraction);
}

template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
}

} // namespace

contracts::Point2 Frame::local(contracts::Point2 point) {
    const double dx = point[0] - origin[0];
    const double dy = point[1] - origin[1];
    return {dx * tangent[0] + dy * tangent[1], dx * normal[0] + dy * normal[1]};
}

Frame frame(compat::Slice<contracts::Point2> support) {
    compat::Slice<double> xs, ys;
    for (const auto point : support) {
        xs.append(point[0]);
        ys.append(point[1]);
    }
    const contracts::Point2 origin{supportPercentile(xs, .5), supportPercentile(ys, .5)};
    double xx = 0, xy = 0, yy = 0;
    for (const auto point : support) {
        const double dx = point[0] - origin[0];
        const double dy = point[1] - origin[1];
        xx += dx * dx;
        xy += dx * dy;
        yy += dy * dy;
    }
    const double eigen = (xx + yy + std::sqrt((xx - yy) * (xx - yy) + 4 * xy * xy)) / 2;
    auto tangent = std::abs(xy) > 1e-12 ? contracts::Point2{eigen - yy, xy} :
        xx >= yy ? contracts::Point2{1., 0.} : contracts::Point2{0., 1.};
    const double length = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]);
    enforce(length > 0, "Degenerate facial frame", 41);
    for (auto& value : tangent) value /= length;
    if (tangent[0] < 0) for (auto& value : tangent) value *= -1;
    Frame result;
    result.origin = origin;
    result.tangent = tangent;
    result.normal = contracts::Point2{-tangent[1], tangent[0]};
    compat::Slice<double> stations;
    for (const auto point : support) stations.append(result.local(point)[0]);
    const double low = supportPercentile(stations, .01);
    const double high = supportPercentile(stations, .99);
    result.width = high - low;
    enforce(result.width > 0, "Degenerate facial width", 46);
    result.origin[0] += (low + high) / 2 * tangent[0];
    result.origin[1] += (low + high) / 2 * tangent[1];
    return result;
}

Frame eyeFrame(compat::Slice<contracts::Point2> support) {
    compat::Slice<double> xs;
    for (const auto point : support) xs.append(point[0]);
    const double low = supportPercentile(xs, .01);
    const double high = supportPercentile(xs, .99);
    // This is local fixed storage of two actual Point2 values, not a new point carrier.
    contracts::Point2 endpoints[2];
    const double locations[2] = {low, high};
    for (std::size_t i = 0; i < 2; ++i) {
        const double x = locations[i];
        compat::Slice<double> ys;
        for (const auto point : support)
            if (std::abs(point[0] - x) <= std::max(2., (high - low) * .015) + 1e-9) ys.append(point[1]);
        endpoints[i] = contracts::Point2{x, supportPercentile(ys, .5)};
    }
    Frame result;
    result.origin = contracts::Point2{(endpoints[0][0] + endpoints[1][0]) / 2,
        (endpoints[0][1] + endpoints[1][1]) / 2};
    result.tangent = contracts::Point2{endpoints[1][0] - endpoints[0][0], endpoints[1][1] - endpoints[0][1]};
    result.width = std::sqrt(result.tangent[0] * result.tangent[0] + result.tangent[1] * result.tangent[1]);
    enforce(result.width > 1e-6, "Degenerate observed eye axis", 64);
    for (auto& value : result.tangent) value /= result.width;
    result.normal = contracts::Point2{-result.tangent[1], result.tangent[0]};
    return result;
}

compat::Slice<double> contactProfile(compat::Slice<contracts::Point2> support, Frame frame, bool lowerEdge) {
    compat::Slice<double> values;
    values.resize(65, std::numeric_limits<double>::quiet_NaN());
    compat::Slice<bool> valid;
    valid.resize(65, false);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double station = -frame.width / 2 + frame.width * i / 64;
        values[i] = lowerEdge ? -std::numeric_limits<double>::infinity() :
            std::numeric_limits<double>::infinity();
        for (const auto point : support) {
            const auto p = frame.local(point);
            if (std::abs(p[0] - station) > frame.width / 128) continue;
            values[i] = lowerEdge ? std::max(values[i], p[1]) : std::min(values[i], p[1]);
            valid[i] = true;
        }
    }
    compat::Slice<std::size_t> known;
    for (std::size_t i = 0; i < 65; ++i) if (valid[i]) known.append(i);
    enforce(!known.empty(), "No painted boundary intersects the eye span", 108);
    for (std::size_t i = 0; i < 65; ++i) if (!valid[i]) {
        if (i < known[0]) values[i] = values[known[0]];
        else if (i > known[known.size() - 1]) values[i] = values[known[known.size() - 1]];
        else for (std::size_t j = 1; j < known.size(); ++j) if (i < known[j]) {
            const double t = static_cast<double>(i - known[j - 1]) / (known[j] - known[j - 1]);
            values[i] = values[known[j - 1]] * (1 - t) + values[known[j]] * t;
            break;
        }
    }
    return values;
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
