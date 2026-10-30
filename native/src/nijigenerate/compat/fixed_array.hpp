#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nijigenerate::compat {

// D floating static arrays own their elements and initialize each to T.init.
template<class T, std::size_t N>
    requires (std::same_as<T, float> || std::same_as<T, double>)
class FixedArray {
public:
    constexpr FixedArray() noexcept {
        values_.fill(std::numeric_limits<T>::quiet_NaN());
    }

    template<class... Values>
        requires (N > 0 && sizeof...(Values) == N && (std::convertible_to<Values, T> && ...))
    constexpr FixedArray(Values&&... values) : values_{static_cast<T>(std::forward<Values>(values))...} {}

    static constexpr std::size_t size() noexcept { return N; }
    constexpr T* data() noexcept { return values_.data(); }
    constexpr const T* data() const noexcept { return values_.data(); }
    constexpr T* begin() noexcept { return values_.data(); }
    constexpr const T* begin() const noexcept { return values_.data(); }
    constexpr T* end() noexcept { return N ? values_.data() + N : values_.data(); }
    constexpr const T* end() const noexcept { return N ? values_.data() + N : values_.data(); }

    constexpr T& operator[](std::size_t index) {
        if (index >= N) throw std::out_of_range("FixedArray index");
        return values_[index];
    }
    constexpr const T& operator[](std::size_t index) const {
        if (index >= N) throw std::out_of_range("FixedArray index");
        return values_[index];
    }

private:
    std::array<T, N> values_;
};

} // namespace nijigenerate::compat
