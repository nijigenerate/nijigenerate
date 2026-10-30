#include "distance1d.hpp"

#include <bit>
#include <vector>

namespace nijigenerate::core::cv::distancetransform {
namespace {
// D int multiplication wraps; signed C++ multiplication would be undefined.
std::int32_t square(std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    return std::bit_cast<std::int32_t>(bits * bits);
}
}

Distance1dResult dt1d(std::span<const float> f) {
    const auto n = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(f.size()));
    const auto count = static_cast<std::size_t>(n);
    auto d = detail::allocateDistanceArray([&] {
        return compat::Slice<float>(std::vector<float>(count, std::numeric_limits<float>::quiet_NaN()));
    });
    auto ind = detail::allocateDistanceArray([&] {
        return compat::Slice<std::int32_t>(std::vector<std::int32_t>(count, 0));
    });
    auto v = detail::allocateDistanceArray([&] { return std::vector<std::int32_t>(count, 0); });
    const auto zCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(n) + 1U);
    auto z = detail::allocateDistanceArray([&] {
        return std::vector<float>(static_cast<std::size_t>(zCount), std::numeric_limits<float>::quiet_NaN());
    });
    detail::distanceAt(z, 0, 22) = -std::numeric_limits<float>::infinity();
    detail::distanceAt(z, 1, 23) = std::numeric_limits<float>::infinity();
    std::int32_t k = 0;

    for (std::int32_t q = 1; q < n; ++q) {
        float s = ((detail::distanceAt(f, q, 27) + static_cast<float>(square(q))) -
            (detail::distanceAt(f, detail::distanceAt(v, k, 27), 27) +
                static_cast<float>(square(detail::distanceAt(v, k, 27))))) /
            (2.0f * static_cast<float>(q - detail::distanceAt(v, k, 27)));
        while (s <= detail::distanceAt(z, k, 28)) {
            --k;
            s = ((detail::distanceAt(f, q, 30) + static_cast<float>(square(q))) -
                (detail::distanceAt(f, detail::distanceAt(v, k, 30), 30) +
                    static_cast<float>(square(detail::distanceAt(v, k, 30))))) /
                (2.0f * static_cast<float>(q - detail::distanceAt(v, k, 30)));
        }
        ++k;
        detail::distanceAt(v, k, 33) = q;
        detail::distanceAt(z, k, 34) = s;
        detail::distanceAt(z, static_cast<std::size_t>(k) + 1, 35) = std::numeric_limits<float>::infinity();
    }

    k = 0;
    for (std::int32_t q = 0; q < n; ++q) {
        while (detail::distanceAt(z, static_cast<std::size_t>(k) + 1, 40) < static_cast<float>(q)) ++k;
        auto& distance = detail::distanceAt(d, q, 42);
        distance = static_cast<float>(square(q - detail::distanceAt(v, k, 42))) +
            detail::distanceAt(f, detail::distanceAt(v, k, 42), 42);
        auto& index = detail::distanceAt(ind, q, 43);
        index = detail::distanceAt(v, k, 43);
    }
    return {d, ind};
}
}
