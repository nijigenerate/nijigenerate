#pragma once

#include "distance1d.hpp"
#include "distance_conversion.hpp"
#include "pixel_operations.hpp"
#include <nijigenerate/compat/contiguous_view.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cfenv>
#include <concepts>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#endif

namespace nijigenerate::core::cv::distancetransform {
// A retained mapping of the source's contiguous output views, not an image API.
template<class T, std::size_t Rank> using DistanceOutput = compat::ContiguousView<T, Rank>;
using DistanceView = DistanceOutput<float, 2>;
using NearestView = DistanceOutput<std::int32_t, 3>;

template<class T> concept DistanceInput = requires(const T& image, std::size_t y, std::size_t x) {
    { image.shape()[0] } -> std::convertible_to<std::size_t>;
    { image.shape()[1] } -> std::convertible_to<std::size_t>;
    { image.at(y, x) == 0 } -> std::convertible_to<bool>;
};

// A mapped user type supplies its actual Source opCast!int / alias-this body.
// A native conversion to size_t is not the Source cast(int) operation.
template<class Value, class = void> struct DistanceDimensionOperations;
template<class Value> struct DistanceDimensionOperations<Value, std::enable_if_t<
    std::is_integral_v<Value> || std::is_enum_v<Value>>> {
    static std::int32_t castInt(Value value) {
        if constexpr (std::is_enum_v<Value>)
            return DistanceDimensionOperations<std::underlying_type_t<Value>>::castInt(
                static_cast<std::underlying_type_t<Value>>(value));
        else if constexpr (std::is_same_v<Value, char> || std::is_same_v<Value, char8_t> ||
            std::is_same_v<Value, char16_t> || std::is_same_v<Value, char32_t> || std::is_same_v<Value, wchar_t>)
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
                static_cast<std::make_unsigned_t<Value>>(value)));
        else return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }
};
template<class Value> struct DistanceDimensionOperations<Value, std::enable_if_t<
    std::is_floating_point_v<Value>>> {
    static std::int32_t castInt(Value value) {
        // The source x64 float/double casts use truncating signed conversion,
        // including integer-indefinite and FE_INVALID for NaN/overflow.
#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        if constexpr (std::same_as<Value, float>)
            return _mm_cvtt_ss2si(_mm_set_ss(value));
        else if constexpr (std::same_as<Value, double>)
            return _mm_cvttsd_si32(_mm_set_sd(value));
#endif
        // Guard the extended/non-SSE conversion before C++'s undefined domain.
        // Values between INT_MIN-1 and INT_MIN truncate to INT_MIN validly;
        // comparing the untruncated value with INT_MIN would reject them.
        if (!std::isfinite(value) || static_cast<long double>(value) <= -2147483649.0L ||
            static_cast<long double>(value) >= 2147483648.0L) {
            (void)std::feraiseexcept(FE_INVALID);
            return (std::numeric_limits<std::int32_t>::min)();
        }
        return static_cast<std::int32_t>(value);
    }
};

// Custom Source shape objects retain their actual opIndex body. The mapped
// built-in arrays below also retain the original compiler bounds behavior.
template<class Shape> struct DistanceShapeOperations {
    template<class Value> static decltype(auto) read(Value&& value, std::size_t index, std::size_t) {
        return std::forward<Value>(value)[index];
    }
};
template<class T, std::size_t N> struct DistanceShapeOperations<std::array<T, N>> {
    template<class Value> static decltype(auto) read(Value&& value, std::size_t index, std::size_t line) {
        return detail::distanceAt(value, index, line);
    }
};
template<class T, std::size_t N> struct DistanceShapeOperations<std::span<T, N>> {
    template<class Value> static decltype(auto) read(Value&& value, std::size_t index, std::size_t line) {
        return detail::distanceAt(value, index, line);
    }
};
template<class T, class Allocator> struct DistanceShapeOperations<std::vector<T, Allocator>> {
    template<class Value> static decltype(auto) read(Value&& value, std::size_t index, std::size_t line) {
        return detail::distanceAt(value, index, line);
    }
};

namespace detail {
template<class Shape> decltype(auto) distanceShapeAt(Shape&& shape, std::size_t index, std::size_t line) {
    return DistanceShapeOperations<std::remove_cvref_t<Shape>>::read(std::forward<Shape>(shape), index, line);
}
template<class Value> std::int32_t distanceNarrow(Value&& value) {
    return DistanceDimensionOperations<std::remove_cvref_t<Value>>::castInt(std::forward<Value>(value));
}
inline std::int32_t distanceProduct(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
inline std::size_t distanceIndex(std::int32_t y, std::int32_t width, std::int32_t x) {
    return static_cast<std::size_t>(std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(distanceProduct(y, width)) + static_cast<std::uint32_t>(x)));
}

// Exact flat contiguous branch of MIR topology.d:1433-1521. Error views keep
// the produced structure and have a null iterator; only the first -1 is inferred.
template<class T, std::size_t Rank>
DistanceOutput<T, Rank> reshapeDistanceOutput(compat::Slice<T> storage,
    const std::array<std::ptrdiff_t, Rank>& requested, std::int32_t& error) {
    std::array<std::size_t, Rank> shape{};
    std::size_t product = 1;
    for (std::size_t i = 0; i < Rank; ++i) {
        shape[i] = static_cast<std::size_t>(requested[i]);
        product *= shape[i];
    }
    const auto count = storage.size();
    if (!count) {
        error = 1; // ReshapeError.empty
        storage.clear();
        return DistanceOutput<T, Rank>(std::move(storage), shape);
    }
    for (std::size_t i = 0; i < Rank; ++i) {
        if (shape[i] == static_cast<std::size_t>(-1)) {
            product = std::size_t{0} - product;
            shape[i] = count / product;
            product *= shape[i];
            break;
        }
    }
    if (count != product) {
        error = 2; // ReshapeError.total
        storage.clear();
    } else error = 0;
    return DistanceOutput<T, Rank>(std::move(storage), shape);
}
}

namespace detail {
template<class T>
void distanceTransformInitialized(const T& binaryImage, DistanceView& dist, NearestView& nearest) {
    const auto height = detail::distanceNarrow(detail::distanceShapeAt(binaryImage.shape(), 0, 78));
    const auto width = detail::distanceNarrow(detail::distanceShapeAt(binaryImage.shape(), 1, 79));
    const auto total = detail::distanceProduct(height, width);
    const auto count = static_cast<std::size_t>(total);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    auto farr = detail::allocateDistanceArray([&] { return std::vector<float>(count, nan); });
    for (std::int32_t y = 0; y < height; ++y) {
        for (std::int32_t x = 0; x < width; ++x) {
            auto& destination = detail::distanceAt(farr, detail::distanceIndex(y, width, x), 87);
            destination = !contours::ContourPixelOperations<
                std::remove_cvref_t<decltype(binaryImage.at(y, x))>>::nonzero(binaryImage.at(y, x))
                ? 0.0f : floatINF;
        }
    }
    auto garr = detail::allocateDistanceArray([&] { return std::vector<float>(count, nan); });
    auto rowNearest = detail::allocateDistanceArray([&] { return std::vector<std::int32_t>(count); });
    for (std::int32_t y = 0; y < height; ++y) {
        auto row = detail::allocateDistanceArray([&] {
            return std::vector<float>(static_cast<std::size_t>(width), nan);
        });
        for (std::int32_t x = 0; x < width; ++x) {
            auto& destination = detail::distanceAt(row, x, 97);
            destination = detail::distanceAt(farr, detail::distanceIndex(y, width, x), 97);
        }
        auto [drow, indrow] = dt1d(row);
        for (std::int32_t x = 0; x < width; ++x) {
            const auto i = detail::distanceIndex(y, width, x);
            auto& distance = detail::distanceAt(garr, i, 103);
            distance = detail::distanceAt(drow, x, 103);
            auto& index = detail::distanceAt(rowNearest, i, 104);
            index = detail::distanceAt(indrow, x, 104);
        }
    }
    auto dtarr = detail::allocateDistanceArray([&] { return std::vector<float>(count, nan); });
    auto nearestArr = detail::allocateDistanceArray([&] {
        return compat::Slice<std::int32_t>(
            std::vector<std::int32_t>(static_cast<std::size_t>(detail::distanceProduct(total, 2))));
    });
    for (std::int32_t x = 0; x < width; ++x) {
        auto col = detail::allocateDistanceArray([&] {
            return std::vector<float>(static_cast<std::size_t>(height), nan);
        });
        for (std::int32_t y = 0; y < height; ++y) {
            auto& destination = detail::distanceAt(col, y, 114);
            destination = detail::distanceAt(garr, detail::distanceIndex(y, width, x), 114);
        }
        auto [dcol, colInd] = dt1d(col);
        for (std::int32_t y = 0; y < height; ++y) {
            const auto i = detail::distanceIndex(y, width, x);
            auto& distance = detail::distanceAt(dtarr, i, 120);
            distance = detail::distanceAt(dcol, y, 120);
            const auto r = detail::distanceAt(colInd, y, 121);
            const auto idx = detail::distanceProduct(detail::distanceNarrow(i), 2);
            auto& column = detail::distanceAt(nearestArr, static_cast<std::size_t>(idx), 125);
            column = detail::distanceAt(rowNearest, detail::distanceIndex(r, width, x), 125);
            detail::distanceAt(nearestArr, detail::distanceIndex(1, idx, 1), 126) = r;
        }
    }
    auto out = detail::allocateDistanceArray([&] { return compat::Slice<float>(std::vector<float>(count, nan)); });
    for (std::int32_t i = 0; i < total; ++i) {
        auto& destination = detail::distanceAt(out, i, 133);
        destination = detail::distanceAt(dtarr, i, 133) != floatINF
            ? std::sqrt(detail::distanceAt(dtarr, i, 133)) : 0;
    }
    std::int32_t error = 0;
    dist = detail::reshapeDistanceOutput<float, 2>(std::move(out), {height, width}, error);
    compat::Slice<std::int32_t> coordinates(std::move(nearestArr));
    nearest = detail::reshapeDistanceOutput<std::int32_t, 3>(std::move(coordinates), {height, width, 2}, error);
}
} // namespace detail

// Ordinary Source `in T` snapshots a value before the out descriptors reset.
template<class T>
void distanceTransform(const T binaryImage, DistanceView& dist, NearestView& nearest) {
    dist = {}; nearest = {};
    detail::distanceTransformInitialized(binaryImage, dist, nearest);
}

// A Source class value is an owned reference. Retain that receiver, not a copy
// of its class fields, and initialize out parameters before dereferencing it.
template<class T>
void distanceTransform(std::shared_ptr<T> binaryImage, DistanceView& dist, NearestView& nearest) {
    dist = {}; nearest = {};
    detail::distanceTransformInitialized(*compat::ngRequireObject(binaryImage), dist, nearest);
}
} // namespace nijigenerate::core::cv::distancetransform
