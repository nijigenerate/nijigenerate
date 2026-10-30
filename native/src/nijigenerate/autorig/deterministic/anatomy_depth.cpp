#include "anatomy.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace nijigenerate::autorig::deterministic::anatomy {
namespace {
// Only builtin byte storage; JSON access, arithmetic and exception construction stay outside.
template<class Operation> auto allocateAnatomyStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}

using core::settings::SettingsJsonValue;

SettingsJsonValue field(const SettingsJsonValue& value, std::string_view key) {
    return value.sourceIndexValue(key);
}

template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) {
        const auto copied = allocateAnatomyStorage([&] {
            return compat::RetainedString(std::string_view(message, N - 1));
        });
        throw compat::ThrowableException(copied,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/anatomy.d"), line);
    }
}

double clamp(double value, double low, double high) {
    return value < low ? low : value > high ? high : value;
}

double supportCoordinate(const compat::Slice<double>& bounds, std::size_t index, std::size_t line) {
    if (index >= bounds.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, bounds.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/anatomy.d"), line);
    return bounds.data()[index];
}

} // namespace

compat::Slice<double> ngRigDepthField(compat::Slice<contracts::Point2> query,
    SettingsJsonValue domain, SettingsJsonValue scaffold, SettingsJsonValue prior) {
    using contracts::ngRigNumber;
    using contracts::ngRigNumbers;
    using contracts::ngRigPoint;
    using contracts::ngRigScalar;
    using detail::length;
    using detail::subtract;

    const auto owner = field(domain, "owner").as_string();
    const auto kind = field(domain, "kind").as_string();
    const auto landmarks = field(scaffold, "landmarks");
    compat::Slice<double> result;
    // Keep distance lexically local to depth evaluation, as in the source.
    const auto distance = [](contracts::Point2 point, contracts::Point2 first, contracts::Point2 last) {
        const auto axis = detail::subtract(last, first);
        const double length2 = detail::dot(axis, axis);
        enforce(length2 > 0, "Degenerate depth axis", 159);
        const double t = clamp(detail::dot(detail::subtract(point, first), axis) / length2, 0., 1.);
        return detail::length({point[0] - first[0] - t * axis[0], point[1] - first[1] - t * axis[1]});
    };
    for (const auto point : query) {
        double z = 0;
        if (owner.view() == "head" || owner.view() == "torso") {
            const auto volume = field(field(scaffold, "volumes"), owner.view());
            const auto center = ngRigPoint(field(volume, "center"));
            const auto radii = ngRigPoint(field(volume, "radii"));
            enforce(radii[0] > 0 && radii[1] > 0, "Nonpositive volume radius", 168);
            const auto f0 = ngRigPoint(field(volume, "frame").sourceIndexValue(std::size_t{0}));
            const auto f1 = ngRigPoint(field(volume, "frame").sourceIndexValue(std::size_t{1}));
            const auto q = subtract(point, center);
            const double u = (q[0] * f0[0] + q[1] * f1[0]) / radii[0];
            const double v = (q[0] * f0[1] + q[1] * f1[1]) / radii[1];
            const double ratio = ngRigNumber(field(field(prior, "depth_ratios"), owner.view()));
            const double scaledRadius = ratio * radii[0];
            const double radial = owner.view() == "head" ? clamp(1 - u * u - v * v, 0., 1.) :
                std::sqrt(clamp(1 - u * u, 0., 1.));
            z = scaledRadius * radial;
        } else {
            const bool arm = owner.view().starts_with("arm:");
            const compat::Slice<compat::RetainedString> roles{
                arm ? compat::RetainedString::fromStaticLiteral("shoulder")
                    : compat::RetainedString::fromStaticLiteral("hip"),
                arm ? compat::RetainedString::fromStaticLiteral("elbow")
                    : compat::RetainedString::fromStaticLiteral("knee"),
                arm ? compat::RetainedString::fromStaticLiteral("wrist")
                    : compat::RetainedString::fromStaticLiteral("ankle")};
            const auto side = field(domain, "side").as_string();
            double d = std::numeric_limits<double>::infinity();
            for (std::size_t i = 0; i < 2; ++i) {
                const auto firstKey = allocateAnatomyStorage([&] {
                    return std::string(roles[i].view()) + "." + std::string(side.view());
                });
                const auto first = ngRigPoint(field(landmarks, firstKey));
                const auto lastKey = allocateAnatomyStorage([&] {
                    return std::string(roles[i + 1].view()) + "." + std::string(side.view());
                });
                const auto last = ngRigPoint(field(landmarks, lastKey));
                const double local = distance(point, first, last);
                if (local < d) d = local;
            }
            const double radius = ngRigNumber(field(domain, "radius"));
            enforce(radius > 0, "Nonpositive limb radius", 185);
            const double ratio = ngRigNumber(field(field(prior, "depth_ratios"), "limb"));
            const double scaledRadius = ratio * radius;
            const double radial = std::sqrt(clamp(1 - d * d / (radius * radius), 0., 1.));
            z = scaledRadius * radial;
        }
        if (kind.view() == "neck") {
            const auto leftShoulder = ngRigPoint(field(landmarks, "shoulder.L"));
            const auto rightShoulder = ngRigPoint(field(landmarks, "shoulder.R"));
            const double width = length(subtract(leftShoulder, rightShoulder)) * .14;
            enforce(width > 0, "Degenerate neck width", 190);
            const auto first = ngRigPoint(field(landmarks, "head_root"));
            const auto last = ngRigPoint(field(landmarks, "neck_base"));
            const double d = distance(point, first, last);
            z = width * std::sqrt(clamp(1 - d * d / (width * width), 0., 1.));
        } else if (kind.view() == "skirt_front" || kind.view() == "skirt_back" || kind.view() == "free_cloth") {
            const auto bounds = ngRigNumbers(field(domain, "support_bounds"));
            const double numerator = point[1] - supportCoordinate(bounds, 1, 195);
            const double upper = supportCoordinate(bounds, 3, 195);
            const double lower = supportCoordinate(bounds, 1, 195);
            double height = 1;
            if (upper - lower > 1) {
                const double currentUpper = supportCoordinate(bounds, 3, 195);
                const double currentLower = supportCoordinate(bounds, 1, 195);
                height = currentUpper - currentLower;
            }
            const double t = clamp(numerator / height, 0., 1.);
            const double right = supportCoordinate(bounds, 2, 196);
            const double left = supportCoordinate(bounds, 0, 196);
            const double radius = (right - left) / 2 * (.45 + .55 * t);
            const double divisor = radius > 1 ? radius : 1;
            const double currentLeft = supportCoordinate(bounds, 0, 197);
            const double currentRight = supportCoordinate(bounds, 2, 197);
            const double q = (point[0] - (currentLeft + currentRight) / 2) / divisor;
            const double ratio = ngRigNumber(field(field(prior, "depth_ratios"), "skirt"));
            const double scaledRadius = ratio * radius;
            const double radial = std::sqrt(clamp(1 - q * q, 0., 1.));
            z = scaledRadius * radial;
            if (kind.view() == "skirt_back") z = -z;
            z += ngRigScalar(domain, compat::RetainedString::fromStaticLiteral("offset"), 0);
        } else if (kind.view() == "back_hair") {
            z = -z - ngRigScalar(domain, compat::RetainedString::fromStaticLiteral("offset"), 0);
        } else if (kind.view() == "appendage") {
            z = ngRigScalar(domain, compat::RetainedString::fromStaticLiteral("offset"), 0);
        } else {
            z += ngRigScalar(domain, compat::RetainedString::fromStaticLiteral("offset"), 0);
        }
        result.append(z);
    }
    return result;
}

} // namespace nijigenerate::autorig::deterministic::anatomy
