#include "neck.hpp"
#include "controls_double_sort.hpp"
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace nijigenerate::autorig::deterministic::neck {
namespace {
using Value = SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using String = core::settings::SettingsString;
template<class T> using Slice = compat::Slice<T>;
template<class T> T minimum(T a, T b) { return b < a ? b : a; }
template<class T> T maximum(T a, T b) { return a < b ? b : a; }
template<std::size_t N> void enforce(bool ok, const char (&message)[N], std::size_t sourceLine) {
    if (!ok) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/neck.d"), sourceLine);
}
Value object(std::initializer_list<std::pair<String, Value>> entries) {
    Object result;
    for (const auto& [key, value] : entries) result[key] = value;
    return Value(result);
}
Value get(Value value, const char* name) {
    return value.sourceIndexValue(name);
}
template<class T> Value numbers(const T& points) {
    Array result;
    const auto count = points.size();
    result.resize(count);
    for (std::size_t index = 0; index < count; ++index) result[index] = Value(points[index]);
    return Value(result);
}
double quantile(std::span<const double> values, double fraction) {
    enforce(!values.empty(), "Empty neck support", 11);
    Slice<double> duplicate(values);
    controls_double_sort::sort({duplicate.data(), duplicate.size()});
    const auto sorted = duplicate.duplicate();
    double index = fraction * (sorted.size() - 1);
    auto first = static_cast<std::size_t>(index);
    auto last = minimum(first + 1, sorted.size() - 1);
    const auto element = [&](std::size_t position) {
        if (position >= sorted.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(position, sorted.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/neck.d"), 16);
        return sorted[position];
    };
    const auto firstValue = element(first);
    const auto firstTerm = firstValue * (1 - (index - first));
    const auto lastValue = element(last);
    const auto lastTerm = lastValue * (index - first);
    return firstTerm + lastTerm;
}
Point2 section(compat::Slice<Point2> points, double fraction) {
    Slice<double> ys;
    for (auto p : points) ys.append(p[1]);
    double low = quantile(ys, .01), high = quantile(ys, .99);
    double y = low + fraction * (high - low);
    Slice<double> xs;
    for (auto p : points) if (std::abs(p[1] - y) <= maximum(1., (high - low) * .025)) xs.append(p[0]);
    if (xs.empty()) {
        Slice<std::size_t> order;
        for (std::size_t i = 0; i < points.size(); ++i) order.append(i);
        compat::phobos_index_sort::sort<sizeof(std::size_t)>(order,
            [&](auto a, auto b) { return std::abs(points[a][1] - y) < std::abs(points[b][1] - y); },
            sizeof(std::size_t) == 8 ? "Failed to sort range of type ulong[]" : "Failed to sort range of type uint[]");
        const auto count = maximum(std::size_t{1}, points.size() / 100);
        if (count > order.size())
            throw compat::ngStaticError<compat::ArraySliceError>(0, count, order.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/neck.d"), 30);
        for (std::size_t j = 0; j < count; ++j) xs.append(points[order[j]][0]);
    }
    return Point2(quantile(xs, .5), y);
}
Point2 project(Point2 point, Point2 origin, Point2 down, Point2 tangent) {
    const auto deltaX = point[0] - origin[0];
    const auto deltaY = point[1] - origin[1];
    Point2 delta(deltaX, deltaY);
    const auto along = delta[0] * down[0] + delta[1] * down[1];
    const auto across = delta[0] * tangent[0] + delta[1] * tangent[1];
    return Point2(along, across);
}
Slice<Point3> profile(std::span<const Point2> cloud, Point2 origin, Point2 down, Point2 tangent,
    double height, double width) {
    Slice<Point2> projected;
    for (auto point : cloud) projected.append(project(point, origin, down, tangent));
    Slice<Point3> rows;
    double center = 0;
    for (int i = 0; i < 129; ++i) {
        double station = height * i / 128;
        Slice<double> cross;
        for (auto p : projected)
            if (std::abs(p[0] - station) <= height * .012 && std::abs(p[1]) <= width * .9) cross.append(p[1]);
        if (cross.size() < 3) continue;
        controls_double_sort::sort({cross.data(), cross.size()});
        bool found = false;
        double bestDistance = std::numeric_limits<double>::infinity();
        double bestLeft = NAN, bestRight = NAN, nextCenter = NAN;
        std::size_t start = 0;
        for (std::size_t end = 1; end < cross.size() + 1; ++end) {
            if (end < cross.size() && cross[end] - cross[end - 1] <= width * .06) continue;
            if (end - start >= 3) {
                auto piece = std::span<const double>(cross).subspan(start, end - start);
                double left = quantile(piece, .05), right = quantile(piece, .95), middle = (left + right) / 2;
                double distance = std::abs(middle - center);
                if (right - left >= width * .08 && std::abs(middle) <= width * .6 &&
                    (!found || distance < bestDistance || (distance == bestDistance && left < bestLeft))) {
                    found = true; bestDistance = distance; bestLeft = left; bestRight = right; nextCenter = middle;
                }
            }
            start = end;
        }
        if (found) { center = nextCenter; rows.append(Point3(station, bestRight - bestLeft, center)); }
    }
    return rows;
}
}

SettingsJsonValue ngRigInferHeadFrame(compat::Slice<Point2> face, Point2 eyeRight, Point2 eyeLeft) {
    Point2 origin((eyeRight[0] + eyeLeft[0]) / 2, (eyeRight[1] + eyeLeft[1]) / 2);
    Point2 tangent(eyeLeft[0] - eyeRight[0], eyeLeft[1] - eyeRight[1]);
    double span = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]);
    enforce(span > 0 && tangent[0] > 0, "Invalid paired eye frame", 83);
    for (auto& v : tangent) v /= span;
    Point2 down(-tangent[1], tangent[0]);
    Slice<double> stations;
    for (auto point : face) stations.append((point[0] - origin[0]) * down[0] + (point[1] - origin[1]) * down[1]);
    auto low = quantile(stations, .01), high = quantile(stations, .99);
    enforce(high > low, "Degenerate head extent", 89);
    return object({{"origin", numbers(origin)}, {"tangent", numbers(tangent)},
        {"normal", numbers(Point2(-down[0], -down[1]))},
        {"head_top", numbers(Point2(origin[0] + low * down[0], origin[1] + low * down[1]))},
        {"head_root", numbers(Point2(origin[0] + high * down[0], origin[1] + high * down[1]))},
        {"method", Value("paired_eye_axis_and_projected_face_extent")}});
}

SettingsJsonValue ngRigInferNeckBase(compat::Slice<Point2> face, compat::Slice<Point2> neck,
    compat::Slice<Point2> torso, compat::Slice<Point2> garments, SettingsJsonValue headFrame) {
    enforce(!face.empty(), "Neck inference needs head support", 102);
    bool sharedFrame = !headFrame.is_null();
    auto origin = sharedFrame ? contracts::ngRigPoint(get(headFrame, "head_root")) : section(face, 1);
    auto top = sharedFrame ? contracts::ngRigPoint(get(headFrame, "head_top")) : section(face, 0);
    Point2 down(origin[0] - top[0], origin[1] - top[1]);
    double height = std::sqrt(down[0] * down[0] + down[1] * down[1]);
    enforce(height > 0, "Degenerate head frame", 108);
    for (auto& v : down) v /= height;
    Point2 tangent(down[1], -down[0]);
    Slice<double> faceU;
    for (auto p : face) faceU.append(project(p, origin, down, tangent)[1]);
    const auto upperWidth = quantile(faceU, .99);
    const auto lowerWidth = quantile(faceU, .01);
    double width = upperWidth - lowerWidth;
    enforce(width > 0, "Degenerate head width", 114);
    auto body = !torso.empty() ? torso : garments;
    Slice<Point2> support;
    for (auto p : neck) support.append(p);
    for (auto p : body) support.append(p);
    enforce(!support.empty(), "Neck inference needs neck or body support", 117);
    auto frame = object({{"origin", numbers(origin)}, {"tangent", numbers(tangent)},
        {"normal", numbers(Point2(-down[0], -down[1]))}});
    auto report = [&](Point2 point, const char* method, const char* structure, double narrow,
        const Slice<Point3>& rows, Value score, const char* provenance) {
        Array samples;
        for (auto row : rows) samples.append(numbers(row));
        auto measured = point;
        if (sharedFrame) {
            auto station = project(point, origin, down, tangent)[0];
            const auto constrainedX = origin[0] + station * down[0];
            const auto constrainedY = origin[1] + station * down[1];
            point = Point2(constrainedX, constrainedY);
        }
        return object({{"xy", numbers(point)}, {"observed_xy", numbers(measured)},
            {"head_axis_constrained", Value(sharedFrame)}, {"method", Value(method)},
            {"structure", Value(structure)}, {"body_support", Value(!torso.empty() ? "skin" : "garment")},
            {"frame", frame}, {"head_height", Value(height)}, {"head_width", Value(width)},
            {"narrow_width", Value(narrow)}, {"width_profile", Value(samples)},
            {"fit_mean_squared_error", score}, {"provenance", Value(provenance)}});
    };
    auto position = [&](double station, double transverse) {
        const auto x = origin[0] + station * down[0] + transverse * tangent[0];
        const auto y = origin[1] + station * down[1] + transverse * tangent[1];
        return Point2(x, y);
    };
    auto narrowSupport = !neck.empty() ? neck : torso;
    auto neckRows = profile(std::span<const Point2>(narrowSupport.data(), narrowSupport.size()),
        origin, down, tangent, height, width);
    if (neckRows.size() >= 4) {
        Slice<double> widths;
        for (auto row : neckRows) widths.append(row[1]);
        double narrow = quantile(widths, .2), broad = quantile(widths, .85);
        if (narrow < width * .75 && broad < narrow * 1.8 && neckRows[neckRows.size() - 1][0] < height * .7) {
            Slice<double> ts;
            for (auto p : narrowSupport) ts.append(project(p, origin, down, tangent)[0]);
            double station = quantile(ts, .99);
            Slice<double> cross;
            for (auto p : narrowSupport) {
                auto uv = project(p, origin, down, tangent);
                if (std::abs(uv[0] - station) <= height * .025) cross.append(uv[1]);
            }
            return report(position(station, quantile(cross, .5)), "narrow_neck_inferior_attachment",
                !neck.empty() ? "separate_narrow_neck_material" : "narrow_neck_in_body_material",
                narrow, neckRows, Value(nullptr), "measured");
        }
    }
    auto rows = profile(support, origin, down, tangent, height, width);
    enforce(!rows.empty(), "No central neck/body cross sections", 161);
    Slice<double> proximal;
    for (auto row : rows) if (row[0] <= height * .45) proximal.append(row[1]);
    double narrow = !proximal.empty() ? quantile(proximal, .2) : 0;
    constexpr auto absent = std::numeric_limits<std::size_t>::max();
    std::size_t first = absent, crossing = absent;
    for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i][1] <= narrow * 1.25) { first = i; break; }
    if (narrow > 0 && narrow < width * .75 && first != absent) {
        for (std::size_t i = first + 3; i < (rows.size() >= 2 ? rows.size() - 2 : 0); ++i)
            if (rows[i][1] >= narrow * 1.5 && rows[i + 1][1] >= narrow * 1.5 && rows[i + 2][1] >= narrow * 1.5) {
                crossing = i; break;
            }
    }
    if (crossing != absent) {
        std::size_t end = rows.size() - 1;
        for (std::size_t i = crossing; i < rows.size(); ++i) if (rows[i][1] >= narrow * 2.2) { end = i; break; }
        auto sample = std::span<const Point3>(rows).subspan(first, minimum(rows.size(), end + 4) - first);
        double bestScore = std::numeric_limits<double>::infinity(), station = NAN;
        std::size_t bestCandidate = 0;
        bool fitted = false;
        for (std::size_t candidate = first + 2; candidate < crossing + 1; ++candidate) {
            double knot = rows[candidate][0], sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (auto row : sample) {
                double x = maximum(0., row[0] - knot), y = row[1];
                sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
            double n = sample.size(), determinant = n * sxx - sx * sx;
            if (determinant <= 0) continue;
            double slope = (n * sxy - sx * sy) / determinant, intercept = (sy - slope * sx) / n;
            if (slope <= 0 || intercept < narrow * .65 || intercept > narrow * 1.5) continue;
            double residual = 0;
            for (auto row : sample) {
                double error = row[1] - intercept - slope * maximum(0., row[0] - knot);
                residual += error * error;
            }
            residual /= n;
            if (residual < bestScore) {
                fitted = true; bestScore = residual; station = knot; bestCandidate = candidate;
            }
        }
        if (fitted) {
            Slice<double> centers;
            auto start = maximum(first, bestCandidate >= 2 ? bestCandidate - 2 : 0);
            for (std::size_t i = start; i < minimum(rows.size(), bestCandidate + 3); ++i) centers.append(rows[i][2]);
            return report(position(station, quantile(centers, .5)), "narrow_neck_to_broad_torso_change",
                !neck.empty() ? "neck_material_includes_body" : "neck_in_body_material",
                narrow, rows, Value(bestScore), "measured");
        }
    }
    Slice<double> upper;
    for (auto p : body) {
        auto uv = project(p, origin, down, tangent);
        if (uv[0] > 0 && std::abs(uv[1]) < width * .45) upper.append(uv[0]);
    }
    enforce(!upper.empty(), "Unresolved neck/body attachment", 214);
    double station = minimum(quantile(upper, .02), height * .55);
    return report(position(station, 0), "occluded_neck_upper_body_support",
        !neck.empty() ? "neck_material_includes_body" : "neck_in_body_material",
        narrow, rows, Value(nullptr), "visual_estimate");
}
}
