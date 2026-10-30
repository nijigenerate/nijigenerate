#include "depth.hpp"
#include "detail/depth_exponential.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>
#include <limits>
#include <functional>
#include <utility>
#include <type_traits>
namespace nijigenerate::autorig::deterministic::depth {
using namespace contracts;
namespace {
constexpr double pi = 0x1.921fb54442d18p+1;
// All three callers have nonnegative bases and finite, strictly positive exponents.
// Match Phobos _powImpl's reachable MSVCRT path, including its integer-power shortcut.
double positivePower(double x, double y) {
    if (std::isnan(x)) return x;
    if (y == 1) return x;
    if (std::isinf(x)) return std::numeric_limits<double>::infinity();
    if (x == 0) return 0.;
    if (x == 1) return 1.;
    if (y >= std::numeric_limits<double>::max()) {
        if (x < 1) return 0.;
        if (x > 1) return std::numeric_limits<double>::infinity();
    }
    if (x >= std::numeric_limits<double>::max()) return std::numeric_limits<double>::infinity();
    if (y < 32768. && std::trunc(y) == y) {
        auto m = static_cast<std::uint32_t>(y);
        if (m == 2) return x * x;
        double p = 1., v = x;
        for (;;) {
            if (m & 1) p *= v;
            m >>= 1; if (!m) break; v *= v;
        }
        return p;
    }
    return detail::exp2(y * detail::log2(x));
}
using Text = compat::RetainedString;
using String = compat::RetainedStringSlice;
[[noreturn]] void fail(Text message, std::size_t line) {
    throw compat::ThrowableException(std::move(message),
        Text::fromStaticLiteral("source/nijigenerate/autorig/deterministic/depth.d"), line);
}
template<std::size_t N> void require(bool ok, const char (&message)[N], std::size_t line) {
    if (!ok) fail(contracts::detail::copyContractMessage(std::string_view(message, N - 1)), line);
}
template<class Message> requires std::is_invocable_v<Message>
void require(bool ok, Message&& message, std::size_t line) {
    if (!ok) {
        const auto evaluated = std::invoke(std::forward<Message>(message));
        fail(contracts::detail::copyContractMessage(std::string_view(evaluated)), line);
    }
}
Json get(Json object, String key, Json fallback = {}) {
    return ngRigGet(object, key, fallback);
}
template<std::size_t N> Json get(Json object, const char (&key)[N], Json fallback = {}) {
    return get(object, Text::fromStaticLiteral(key), fallback);
}
Json member(Json object, std::string_view key) {
    return object.sourceIndexValue(key);
}
template<std::size_t N, std::size_t M>
String text(Json object, const char (&key)[N], const char (&fallback)[M]) {
    return ngRigString(object, Text::fromStaticLiteral(key), Text::fromStaticLiteral(fallback));
}
double scalar(Json v, Json parameters) { return ngRigNumber(v.is_string() ? member(parameters, v.as_string().view()) : v); }
template<std::size_t N> double value(Json op, const char (&key)[N], Json parameters, double fallback) {
    return scalar(get(op, key, Json(fallback)), parameters);
}
template<std::size_t N> Point2 pair(Json record, const char (&key)[N], Json parameters, Point2 fallback) {
    auto entries = get(record, key, core::settings::SettingsJsonArray{Json(fallback[0]), Json(fallback[1])}).as_array();
    require(entries.size() == 2, "Expected a depth operator pair", 67);
    return {scalar(entries[0], parameters), scalar(entries[1], parameters)};
}
using Allowed = compat::Slice<Text>;
template<std::size_t N> Text literal(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
void known(Json op, Allowed allowed) {
    auto& object = op.sourceObjectRef();
    for (const auto item : object) {
        const auto key = item.key();
        bool found = key == "id" || key == "label" || key == "description";
        for (auto candidate : allowed) if (key == candidate.view()) found = true;
        require(found, [&] {
            return contracts::detail::allocateContractStorage([&] {
                return "Unknown depth operator field: " + std::string(key);
            });
        }, 75);
    }
}
template<std::size_t N>
double profile(double coordinate, Json op, const char (&key)[N], Json parameters, double fallback) {
    auto points = get(op, key); if (points.is_null()) return fallback;
    auto entries = points.as_array(); require(!entries.empty(), "Empty depth profile", 82);
    compat::Slice<Point2> parsed;
    for (auto entry : entries) {
        auto values = entry.as_array(); require(values.size() == 2, "Invalid depth profile pair", 85);
        Point2 p{scalar(values[0], parameters), scalar(values[1], parameters)};
        require(p[0] >= 0 && p[0] <= 1 && (parsed.empty() || p[0] > parsed[parsed.size() - 1][0]), "Unordered depth profile", 87);
        parsed.append(p);
    }
    if (coordinate <= parsed[0][0]) return parsed[0][1];
    for (std::size_t i = 1; i < parsed.size(); ++i) if (coordinate <= parsed[i][0]) {
        double t = (coordinate - parsed[i - 1][0]) / (parsed[i][0] - parsed[i - 1][0]);
        return parsed[i - 1][1] * (1 - t) + parsed[i][1] * t;
    }
    return parsed[parsed.size() - 1][1];
}
double section(Point2 p, Json op, Json parameters, bool swept, bool flat = false) {
    double center = value(op, "center_u", parameters, .5), radius = value(op, "radius_u", parameters, .5);
    if (swept) {
        center = profile(p[1], op, "center_u_profile", parameters, center);
        radius = profile(p[1], op, "radius_u_profile", parameters, radius);
    }
    require(radius > 0, "Nonpositive depth section radius", 104);
    auto crossDescriptor = flat ? String(Text::fromStaticLiteral("flat")) : text(op, "cross_section", "ellipse");
    const auto cross = crossDescriptor.view();
    double factor = 1;
    if (cross == "ellipse") {
        double exponent = value(op, "exponent", parameters, .5);
        require(exponent > 0, "Nonpositive depth section exponent", 109);
        double q = (p[0] - center) / radius; factor = positivePower(std::max(0., 1 - q * q), exponent);
    } else if (cross == "angular") {
        auto theta = pair(op, "theta_degrees", parameters, {-90., 90.});
        require(theta[0] != theta[1], "Degenerate angular depth interval", 114);
        factor = std::cos((theta[0] + p[0] * (theta[1] - theta[0])) * pi / 180);
    } else require(cross == "flat", "Unknown depth cross section", 116);
    const double base = value(op, "base", parameters, 0);
    const double centerDepth = swept ? profile(p[1], op, "center_depth_profile", parameters, 0) : 0;
    const double baseDepth = base + centerDepth;
    const double depth = value(op, "depth", parameters, 1);
    const double depthProfile = profile(p[1], op, "depth_profile", parameters, 1);
    return baseDepth + depth * depthProfile * factor;
}
}
double ngRigCompactSupport(Point2 p, Json op, Json parameters) {
    auto center = pair(op, "center", parameters, {.5, .5}), radius = pair(op, "radius", parameters, {.5, .5});
    require(radius[0] > 0 && radius[1] > 0, "Nonpositive compact support radius", 124);
    double angle = value(op, "rotation_degrees", parameters, 0) * pi / 180;
    double dx = p[0] - center[0], dy = p[1] - center[1];
    double x = (dx * std::cos(angle) + dy * std::sin(angle)) / radius[0];
    double y = (-dx * std::sin(angle) + dy * std::cos(angle)) / radius[1];
    double distance = std::sqrt(x * x + y * y), remaining = std::max(0., 1 - distance);
    // Phobos pow(double, int) squares twice, then multiplies its accumulator by the fourth power.
    double fourth = remaining * remaining; fourth *= fourth;
    return (1.0 * fourth) * (4 * distance + 1);
}
compat::Slice<Point2> ngRigLocalCorrections(compat::Slice<Point2> positions, compat::Slice<Point2> material,
    Point3 pose, Point3 neutral, Json rules, Json parameters, double scale) {
    require(positions.size() == material.size() && std::isfinite(scale) && scale > 0, "Invalid correction dimensions", 15);
    auto result = positions.duplicate();
    auto basis = [&](Point3 angles, Json driver) {
        auto axisDescriptor = text(driver, "axis", "yaw"), kindDescriptor = text(driver, "basis", "sin");
        const auto axis = axisDescriptor.view(), kind = kindDescriptor.view();
        require(axis == "yaw" || axis == "pitch" || axis == "roll", "Unknown correction axis", 19);
        const double radians = angles[axis == "yaw" ? 0 : axis == "pitch" ? 1 : 2] * pi / 180;
        const double driverScale = value(driver, "scale", parameters, 1);
        double x = radians * driverScale;
        if (kind == "linear") return x;
        if (kind == "sin") return std::sin(x);
        if (kind == "sin2") return std::sin(x) * std::sin(x);
        require(kind == "positive_sin", "Unknown correction basis", 24); return std::max(0., std::sin(x));
    };
    for (auto rule : rules.as_array()) {
        require(text(rule, "type", "local").view() == "local", "Unknown correction operator", 28);
        known(rule, {literal("type"), literal("center"), literal("radius"), literal("rotation_degrees"),
            literal("direction"), literal("amplitude"), literal("driver"), literal("pin_edges")});
        auto direction = pair(rule, "direction", parameters, {1., 0.});
        double norm = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
        require(norm > 0, "Degenerate correction direction", 32); for (auto& v : direction) v /= norm;
        auto driver = get(rule, "driver", core::settings::SettingsJsonObject{});
        known(driver, {literal("axis"), literal("basis"), literal("scale")});
        const double poseBasis = basis(pose, driver);
        const double neutralBasis = basis(neutral, driver);
        double response = poseBasis - neutralBasis;
        double amplitude = scalar(member(rule, "amplitude"), parameters) * scale * response;
        compat::Slice<String> edges;
        for (auto entry : get(rule, "pin_edges", core::settings::SettingsJsonArray{}).as_array()) {
            String edge = entry.as_string();
            const auto edgeView = edge.view();
            require(edgeView == "u0" || edgeView == "u1" || edgeView == "v0" || edgeView == "v1", "Unknown pinned edge", 40);
            for (auto previous : edges) require(previous != edge, "Duplicate pinned edge", 41);
            edges.append(edge);
        }
        for (std::size_t i = 0; i < material.size(); ++i) {
            auto uv = material[i]; require(uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1, "Correction outside material UV", 45);
            double weight = ngRigCompactSupport(uv, rule, parameters);
            for (auto edge : edges) {
                double coordinate = uv[edge.view()[0] == 'v']; if (edge.view()[1] == '1') coordinate = 1 - coordinate;
                weight *= coordinate * coordinate;
            }
            for (std::size_t axis = 0; axis < 2; ++axis) {
                result[i][axis] += weight * amplitude * direction[axis];
                require(std::isfinite(result[i][axis]), "Nonfinite corrected point", 54);
            }
        }
    }
    return result;
}
compat::Slice<double> ngRigEvaluateDepth(compat::Slice<Point2> query, Json operators, Json parameters,
    compat::Slice<double> host) {
    require(parameters.is_object(), "Depth parameters must be an object", 134);
    const Allowed sectionFields{literal("type"), literal("depth"), literal("depth_profile"), literal("center_u"),
        literal("radius_u"), literal("exponent"), literal("base"), literal("cross_section"), literal("theta_degrees")};
    compat::Slice<double> result;
    result.resize(query.size(), 0.);
    for (auto p : query) require(p[0] >= 0 && p[0] <= 1 && p[1] >= 0 && p[1] <= 1, "Depth query outside material UV", 138);
    for (auto op : operators.as_array()) {
        auto kindDescriptor = member(op, "type").as_string();
        const auto kind = kindDescriptor.view();
        bool ordinary = kind == "section_surface" || kind == "section" || kind == "flared_shell";
        bool swept = kind == "swept_surface" || kind == "swept_tube" || kind == "ribbon";
        if (ordinary || swept) {
            const Allowed extra = swept ? Allowed{literal("center_depth_profile"),
                literal("center_u_profile"), literal("radius_u_profile")} : Allowed{};
            Allowed fields;
            fields.resize(sectionFields.size() + extra.size());
            for (std::size_t i = 0; i < sectionFields.size(); ++i) fields[i] = sectionFields[i];
            for (std::size_t i = 0; i < extra.size(); ++i) fields[sectionFields.size() + i] = extra[i];
            known(op, fields);
        } else if (kind == "compact_relief") known(op, {literal("type"), literal("center"), literal("radius"),
            literal("height"), literal("rotation_degrees")});
        else if (kind == "host_offset") known(op, {literal("type"), literal("offset")});
        else if (kind == "constant") known(op, {literal("type"), literal("value")});
        else if (kind == "bend") known(op, {literal("type"), literal("amplitude"), literal("axis"),
            literal("power"), literal("origin")});
        else if (kind == "wave") known(op, {literal("type"), literal("amplitude"), literal("axis"),
            literal("frequency"), literal("phase"), literal("fade_axis"), literal("fade_power")});
        else if (kind == "ribbon_network") known(op, {literal("type"), literal("ribbons"), literal("outside_depth")});
        else {
            const auto message = contracts::detail::allocateContractStorage([&] {
                return Text("Unknown template depth operator: " + std::string(kind));
            });
            fail(message, 151);
        }
        if (kind == "host_offset") require(host.size() == query.size() && !host.isNull(), "Missing explicit host depth", 152);
        for (std::size_t i = 0; i < query.size(); ++i) {
            auto p = query[i]; double addition = 0;
            if (ordinary || swept) addition = section(p, op, parameters, swept);
            else if (kind == "compact_relief") {
                const double height = scalar(member(op, "height"), parameters);
                const double support = ngRigCompactSupport(p, op, parameters);
                addition = height * support;
            } else if (kind == "host_offset") {
                const double hostDepth = host[i];
                addition = hostDepth + value(op, "offset", parameters, 0);
            }
            else if (kind == "constant") addition = value(op, "value", parameters, 0);
            else if (kind == "bend") {
                auto axisDescriptor = text(op, "axis", "v");
                const auto axis = axisDescriptor.view();
                require(axis == "u" || axis == "v", "Unknown bend axis", 161);
                double power = value(op, "power", parameters, 2); require(power > 0, "Nonpositive bend power", 163);
                double distance = p[axis == "v"] - value(op, "origin", parameters, 0);
                const double amplitude = scalar(member(op, "amplitude"), parameters);
                const double signedAmplitude = amplitude * (distance < 0 ? -1 : distance > 0 ? 1 : 0);
                addition = signedAmplitude * positivePower(std::abs(distance), power);
            } else if (kind == "wave") {
                auto axisDescriptor = text(op, "axis", "u");
                const auto axis = axisDescriptor.view();
                require(axis == "u" || axis == "v", "Unknown wave axis", 168);
                double fade = 1; auto fadeAxis = get(op, "fade_axis");
                if (!fadeAxis.is_null()) {
                    auto fadeDescriptor = fadeAxis.as_string();
                    const auto a = fadeDescriptor.view();
                    require(a == "u" || a == "v", "Unknown wave fade axis", 172);
                    double power = value(op, "fade_power", parameters, 1); require(power > 0, "Nonpositive wave fade power", 174);
                    fade = positivePower(p[a == "v"], power);
                }
                const double amplitude = scalar(member(op, "amplitude"), parameters) * fade;
                const double frequency = value(op, "frequency", parameters, 1);
                const double oscillation = frequency * p[axis == "v"];
                const double phase = value(op, "phase", parameters, 0);
                addition = amplitude * std::sin((2 * pi) * (oscillation + phase));
            } else if (kind == "ribbon_network") {
                auto ribbons = member(op, "ribbons").as_array(); require(!ribbons.empty(), "Empty ribbon network", 180);
                bool assigned = false; addition = value(op, "outside_depth", parameters, 0);
                for (auto ribbon : member(op, "ribbons").as_array()) {
                    known(ribbon, {literal("support"), literal("depth"), literal("depth_profile"), literal("base"),
                        literal("center_depth_profile")});
                    auto box = ngRigNumbers(get(ribbon, "support", core::settings::SettingsJsonArray{Json(0.), Json(0.), Json(1.), Json(1.)}));
                    require(box.size() == 4 && box[0] >= 0 && box[1] >= 0 && box[2] <= 1 && box[3] <= 1
                        && box[2] > box[0] && box[3] > box[1], "Invalid ribbon support rectangle", 186);
                    if (p[0] < box[0] || p[0] > box[2] || p[1] < box[1] || p[1] > box[3]) continue;
                    Point2 local{(p[0] - box[0]) / (box[2] - box[0]), (p[1] - box[1]) / (box[3] - box[1])};
                    double z = section(local, ribbon, parameters, true, true);
                    require(!assigned || std::abs(addition - z) <= 1e-9, "Incompatible overlapping ribbon depths", 191);
                    assigned = true; addition = z;
                }
            }
            result[i] += addition; require(std::isfinite(result[i]), "Nonfinite template depth", 196);
        }
    }
    return result;
}
}
