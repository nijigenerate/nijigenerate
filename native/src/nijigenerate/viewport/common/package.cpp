#include "package.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace nijigenerate::viewport::common {
namespace {
namespace math = nicxlive::core::math;

#if defined(_WIN32) && defined(_MSC_VER)
// LDC's default MSVC target maps D real to IEEE binary64.
using SourceReal = double;
static_assert(sizeof(SourceReal) == 8 && std::numeric_limits<SourceReal>::digits == 53);
constexpr SourceReal sourcePi = 0x1.921fb54442d18p+1;
#else
// Other LDC default targets follow their native C long-double ABI.
using SourceReal = long double;
constexpr SourceReal sourcePi = 0x1.921fb54442d18469898cc51701b84p+1L;
#endif

float circleAngle(std::uint64_t index, std::uint32_t segments) {
    const auto twiceIndex = index * std::uint64_t{2};
    const SourceReal product = static_cast<SourceReal>(twiceIndex) * sourcePi;
    const SourceReal quotient = product / static_cast<SourceReal>(segments);
    return static_cast<float>(quotient);
}
} // namespace

nicxlive::core::math::Vec3Array ngCreateCircleBuffer(nicxlive::core::math::Vec2 origin,
    nicxlive::core::math::Vec2 radii, std::uint32_t segments) {
    math::Vec3Array lines;
    const auto addPoint = [&](std::uint64_t index) {
        const auto theta = circleAngle(index, segments);
        const float xOffset = radii.x * std::sin(theta);
        const float yOffset = radii.y * std::cos(theta);
        const math::Vec2 point{origin.x + xOffset, origin.y + yOffset};
        lines.append(math::Vec3{point.x, point.y, 0.0f});
    };
    for (std::uint32_t index = 0; index < segments; ++index) {
        addPoint(index);
        addPoint(static_cast<std::uint64_t>(index) + 1);
    }
    return lines;
}

nicxlive::core::math::Vec3Array ngCreateRectBuffer(nicxlive::core::math::Vec2 from,
    nicxlive::core::math::Vec2 to) {
    return math::Vec3Array{math::Vec3{from.x, from.y, 0.0f}, math::Vec3{to.x, from.y, 0.0f},
        math::Vec3{to.x, from.y, 0.0f}, math::Vec3{to.x, to.y, 0.0f},
        math::Vec3{to.x, to.y, 0.0f}, math::Vec3{from.x, to.y, 0.0f},
        math::Vec3{from.x, to.y, 0.0f}, math::Vec3{from.x, from.y, 0.0f}};
}

nicxlive::core::math::Vec3Array ngCreateLineBuffer(nicxlive::core::math::Vec2 from,
    nicxlive::core::math::Vec2 to) {
    return math::Vec3Array{math::Vec3{from.x, from.y, 0.0f}, math::Vec3{to.x, to.y, 0.0f}};
}

} // namespace nijigenerate::viewport::common
