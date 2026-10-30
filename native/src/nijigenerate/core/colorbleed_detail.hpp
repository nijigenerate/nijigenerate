#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/native_arithmetic_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <tuple>
#include <common/shared_array.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nijigenerate::core::colorbleed::detail {

inline constexpr int TOPROCESS = 0;
inline constexpr int INPROCESS = 1;
inline constexpr int PIXELDATA = 2;

struct Pixel {
    std::uint8_t r{}, g{}, b{}, a{};
};
static_assert(sizeof(Pixel) == 4 && alignof(Pixel) == 1);
static_assert(offsetof(Pixel, r) == 0 && offsetof(Pixel, g) == 1
              && offsetof(Pixel, b) == 2 && offsetof(Pixel, a) == 3);

struct Mask {
    compat::Slice<std::uint8_t> data;
    compat::Slice<std::size_t> pending;
    compat::Slice<std::size_t> changing;

    explicit Mask(std::span<const std::uint8_t> texture);
    std::int32_t getPendingSize() const;
    std::uint8_t getMask(std::size_t index) const;
    std::size_t removeIndex(std::size_t index);
};

struct MaskIterator {
    Mask* mask{};
    std::size_t index{};
    explicit MaskIterator(Mask* value);
    bool hasNext() const;
    std::int32_t next();
    void markAsInProgress();
    void reset();
};

inline std::int32_t narrowIndex(std::size_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
inline std::int32_t add(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) + std::uint32_t(right));
}
inline std::int32_t multiply(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) * std::uint32_t(right));
}

inline compat::Slice<std::uint8_t> allocateMaskData(std::size_t count) {
    try {
        return compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(count));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline Mask::Mask(std::span<const std::uint8_t> texture)
    : data(texture.size() < 4 ? compat::Slice<std::uint8_t>{}
                            : allocateMaskData(texture.size() / 4)) {
    for (std::size_t index = 0; index < data.size(); ++index) {
        if (texture[index * 4 + 3] == 0) {
            data[index] = TOPROCESS;
            pending.append(index);
        } else {
            data[index] = PIXELDATA;
        }
    }
}

} // namespace nijigenerate::core::colorbleed::detail

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::core::colorbleed::detail::Pixel> {
    using Value = ::nijigenerate::core::colorbleed::detail::Pixel;
    inline static constexpr std::string_view typeName = "Pixel";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("r", offsetof(Value, r), &Value::r),
            ngFormatAggregateMember("g", offsetof(Value, g), &Value::g),
            ngFormatAggregateMember("b", offsetof(Value, b), &Value::b),
            ngFormatAggregateMember("a", offsetof(Value, a), &Value::a)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::core::colorbleed::detail::Mask> {
    using Value = ::nijigenerate::core::colorbleed::detail::Mask;
    inline static constexpr std::string_view typeName = "Mask";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("data", offsetof(Value, data), &Value::data),
            ngFormatAggregateMember("pending", offsetof(Value, pending), &Value::pending),
            ngFormatAggregateMember("changing", offsetof(Value, changing), &Value::changing)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::core::colorbleed::detail::MaskIterator> {
    using Value = ::nijigenerate::core::colorbleed::detail::MaskIterator;
    inline static constexpr std::string_view typeName = "MaskIterator";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("mask", offsetof(Value, mask), &Value::mask),
            ngFormatAggregateMember("index", offsetof(Value, index), &Value::index)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::colorbleed::detail::Pixel> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.colorbleed.Pixel)";
    inline static constexpr char unqualifiedArgument[] = "(Pixel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.colorbleed.Pixel";
    inline static constexpr char unqualifiedElement[] = "Pixel";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::colorbleed::detail::Mask> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.colorbleed.Mask)";
    inline static constexpr char unqualifiedArgument[] = "(Mask)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.colorbleed.Mask";
    inline static constexpr char unqualifiedElement[] = "Mask";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::colorbleed::detail::MaskIterator> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.colorbleed.MaskIterator)";
    inline static constexpr char unqualifiedArgument[] = "(MaskIterator)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.colorbleed.MaskIterator";
    inline static constexpr char unqualifiedElement[] = "MaskIterator";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::colorbleed::detail {
inline std::int32_t Mask::getPendingSize() const { return narrowIndex(pending.size()); }
inline std::uint8_t Mask::getMask(std::size_t index) const {
    if (index >= data.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, data.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/colorbleed.d"), 121);
    return data[index];
}
inline std::size_t Mask::removeIndex(std::size_t index) {
    if (index >= pending.size()) throw compat::ThrowableException(
        compat::RetainedString::fromStaticLiteral("Out of bounds write to mask"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/colorbleed.d"), 126);
    const auto value = pending[index];
    pending[index] = pending[pending.size() - 1];
    pending.resize(pending.size() - 1);
    return value;
}
inline MaskIterator::MaskIterator(Mask* value) : mask(value) {}
inline bool MaskIterator::hasNext() const {
    return index < static_cast<std::size_t>(compat::ngRequireObject(mask)->getPendingSize());
}
inline std::int32_t MaskIterator::next() {
    const auto& pending = compat::ngRequireObject(mask)->pending;
    const auto current = index++;
    if (current >= pending.size()) throw compat::ngStaticError<compat::ArrayIndexError>(current, pending.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/colorbleed.d"), 150);
    return narrowIndex(pending[current]);
}
inline void MaskIterator::markAsInProgress() {
    --index;
    const auto removed = compat::ngRequireObject(mask)->removeIndex(index);
    compat::ngRequireObject(mask)->changing.append(removed);
}
inline void MaskIterator::reset() {
    index = 0;
    const auto current = compat::ngRequireObject(mask);
    for (const auto changed : current->changing) {
        if (changed >= current->data.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            changed, current->data.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/colorbleed.d"), 163);
        current->data[changed] = PIXELDATA;
    }
    current->changing.resize(0);
}

// Byte operations preserve packed RGBA layout without C++ union type punning.
class PixelBytes {
public:
    explicit PixelBytes(std::span<std::uint8_t> bytes) : data_(bytes.data()), length_(bytes.size()) {}
    explicit PixelBytes(nicxlive::core::common::SharedArray<std::uint8_t> bytes)
        : owner_(std::move(bytes)), data_(owner_.data()), length_(owner_.size()) {}
    Pixel get(std::size_t index) const {
        check(index, 202);
        const auto offset = index * 4;
        return {data_[offset], data_[offset + 1], data_[offset + 2], data_[offset + 3]};
    }
    void set(std::size_t index, Pixel pixel) const {
        check(index, 212);
        const auto offset = index * 4;
        data_[offset] = pixel.r;
        data_[offset + 1] = pixel.g;
        data_[offset + 2] = pixel.b;
        data_[offset + 3] = pixel.a;
    }
private:
    void check(std::size_t index, std::size_t sourceLine) const {
        if (index >= length_) throw compat::ngStaticError<compat::ArrayIndexError>(index, length_,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/colorbleed.d"), sourceLine);
    }
    // The source union checks its numeric Pixel length, not the original byte
    // view's length divided by four. Keep the actual byte owner and pointer;
    // valid cells can belong to a larger allocation, including external storage.
    nicxlive::core::common::SharedArray<std::uint8_t> owner_;
    std::uint8_t* data_;
    std::size_t length_;
};

inline void executeIteration(PixelBytes rgba, Mask* mask, std::int32_t width, std::int32_t height) {
    constexpr std::array<std::array<int, 2>, 8> offsets{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
    MaskIterator iterator(mask);
    while (iterator.hasNext()) {
        const auto pixelIndex = iterator.next();
        // Preserve the native fault at the original integer remainder operation.
        if (width == 0 || (pixelIndex == INT32_MIN && width == -1))
            compat::ngFailIntegerDivideByZero();
        const auto x = pixelIndex % width;
        const auto y = pixelIndex / width;
        int red = 0, green = 0, blue = 0, count = 0;
        for (const auto offset : offsets) {
            const auto column = add(x, offset[0]);
            const auto row = add(y, offset[1]);
            if (column < 0 || column >= width || row < 0 || row >= height) continue;
            const auto current = add(multiply(row, width), column);
            const auto pixel = rgba.get(static_cast<std::size_t>(current));
            if (mask->getMask(static_cast<std::size_t>(current)) == PIXELDATA) {
                red += pixel.r;
                green += pixel.g;
                blue += pixel.b;
                ++count;
            }
        }
        if (count != 0) {
            rgba.set(static_cast<std::size_t>(pixelIndex), {
                static_cast<std::uint8_t>(red / count), static_cast<std::uint8_t>(green / count),
                static_cast<std::uint8_t>(blue / count), 0});
            iterator.markAsInProgress();
        }
    }
    iterator.reset();
}

}
