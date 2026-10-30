#pragma once
// Copyright The D Language Foundation 2000-2013. BSL-1.0;
// retained license: native/port/licenses/phobos-float-format.txt.
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include "typed_format_layout.hpp"
#include "typed_format_grapheme.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <bit>
#include <climits>
#include <functional>
#include <concepts>
#include <string>
#include <span>
#include <type_traits>
#include <utility>

namespace nijigenerate::core::translation {
namespace detail {
struct ParsedTypedFormat;
}

// These are the Source bitfield properties over the one actual flags byte.
class FormatFlag {
    std::uint8_t snapshot_ = 0;
    std::uint8_t* flags_;
    std::uint8_t mask_;
public:
    FormatFlag(std::uint8_t& flags, std::uint8_t mask) noexcept : flags_(&flags), mask_(mask) {}
    FormatFlag(const FormatFlag& other) noexcept
        : snapshot_(static_cast<bool>(other) ? 1 : 0), flags_(&snapshot_), mask_(1) {}
    operator bool() const noexcept { return (*flags_ & mask_) != 0; }
    FormatFlag& operator=(bool value) noexcept {
        *flags_ = static_cast<std::uint8_t>(value ? *flags_ | mask_ : *flags_ & ~mask_);
        return *this;
    }
    FormatFlag& operator=(const FormatFlag& value) noexcept { return *this = static_cast<bool>(value); }
};

template<class Writer> void ngTypedFormatPut(Writer& writer, compat::RetainedStringSlice text) {
    if constexpr (requires { writer.put(text); }) writer.put(text);
    else if constexpr (requires { writer.put(text.view()); }) writer.put(text.view());
    else if constexpr (std::invocable<Writer&, compat::RetainedStringSlice>) std::invoke(writer, text);
    else if constexpr (std::invocable<Writer&, std::string_view>) std::invoke(writer, text.view());
    else if constexpr (std::invocable<Writer&, char>)
        for (char value : text.view()) std::invoke(writer, value);
    else for (char value : text.view()) writer.put(value);
}

template<class Writer> void ngTypedFormatPut(Writer& writer, char value) {
    if constexpr (requires { writer.put(value); }) writer.put(value);
    else if constexpr (std::invocable<Writer&, char>) std::invoke(writer, value);
    else ngTypedFormatPut(writer, compat::RetainedStringSlice(
        compat::RetainedString::fromBorrowedView(std::string_view(&value, 1))));
}

namespace detail {
// The actual default %s writeAligned program has four put expressions, even
// when prefix, grouped content, fractional content or suffix is empty.
template<class Writer> void putDefaultAlignedText(Writer& writer, compat::RetainedStringSlice text) {
    // formatRange slices byte strings by the actual default precision sentinel.
    constexpr auto precision = static_cast<std::size_t>(INT_MAX - 1);
    if (text.size() > precision) text = text.slice(0, precision);
    const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
    ngTypedFormatPut(writer, empty);
    ngTypedFormatPut(writer, empty);
    ngTypedFormatPut(writer, empty);
    ngTypedFormatPut(writer, text);
}

template<class Writer> void putDefaultInteger(Writer& writer, std::uint64_t value,
    bool negative, unsigned base) {
    // Phobos formatValueImplUlong generates the digits before writeAligned.
    std::array<char, 64> digits;
    auto position = digits.size();
    do {
        const auto digit = static_cast<unsigned>(value % base);
        digits[--position] = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
        value /= base;
    } while (value);
    const compat::RetainedStringSlice grouped(compat::RetainedString::fromBorrowedView(
        std::string_view(digits.data() + position, digits.size() - position)));
    std::array<char, 3> sign;
    if (negative) sign[2] = '-';
    const compat::RetainedStringSlice prefix(compat::RetainedString::fromBorrowedView(
        std::string_view(sign.data() + 2, negative ? 1 : 0)));
    const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
    ngTypedFormatPut(writer, prefix);
    ngTypedFormatPut(writer, grouped);
    ngTypedFormatPut(writer, empty);
    ngTypedFormatPut(writer, empty);
}
} // namespace detail

// Stream the real default %s scalar/string domain, without formatting a whole
// field and splitting its result. No mutable external FormatSpec is captured.
template<class Writer, class T> requires (std::integral<T> && sizeof(T) <= sizeof(std::uint64_t) &&
    !std::same_as<T, char16_t> && !std::same_as<T, char32_t> && !std::same_as<T, wchar_t>)
void ngFormatDefaultValueTo(Writer& writer, T value) {
    if constexpr (std::same_as<T, bool>) {
        detail::putDefaultAlignedText(writer, compat::RetainedStringSlice(value
            ? compat::RetainedString::fromStaticLiteral("true")
            : compat::RetainedString::fromStaticLiteral("false")));
    } else if constexpr (std::same_as<T, char> || std::same_as<T, char8_t>) {
        const char character = static_cast<char>(value);
        detail::putDefaultAlignedText(writer, compat::RetainedStringSlice(
            compat::RetainedString::fromBorrowedView(std::string_view(&character, 1))));
    } else {
        using Unsigned = std::make_unsigned_t<T>;
        const auto raw = static_cast<Unsigned>(value);
        const bool negative = std::is_signed_v<T> && value < 0;
        const auto magnitude = negative ? static_cast<Unsigned>(Unsigned{0} - raw) : raw;
        detail::putDefaultInteger(writer, static_cast<std::uint64_t>(magnitude), negative, 10);
    }
}
template<class Writer> void ngFormatDefaultValueTo(Writer& writer, const void* value) {
    if (value) detail::putDefaultInteger(writer,
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value)), false, 16);
    else detail::putDefaultAlignedText(writer,
        compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("null")));
}
template<class Writer> void ngFormatDefaultValueTo(Writer& writer, compat::RetainedStringSlice value) {
    detail::putDefaultAlignedText(writer, std::move(value));
}
template<class Writer> void ngFormatDefaultValueTo(Writer& writer, compat::RetainedString value) {
    detail::putDefaultAlignedText(writer, compat::RetainedStringSlice(std::move(value)));
}

class FormatSpec {
public:
    static constexpr int DYNAMIC = INT_MAX;
    static constexpr int UNSPECIFIED = DYNAMIC - 1;
    int width = 0;
    int precision = UNSPECIFIED;
    int separators = UNSPECIFIED;
    bool dynamicSeparatorChar = false;
    char32_t separatorChar = ',';
    char spec = 's';
    std::uint16_t indexStart = 0;
    std::uint16_t indexEnd = 0;
    std::uint8_t allFlags = 0;
    FormatFlag flDash{allFlags, 1}, flZero{allFlags, 2}, flSpace{allFlags, 4}, flPlus{allFlags, 8};
    FormatFlag flHash{allFlags, 16}, flEqual{allFlags, 32}, flSeparator{allFlags, 64};
    compat::RetainedStringSlice nested, sep, trailing;
    inline static const auto seqBefore = compat::RetainedString::fromStaticLiteral("[");
    inline static const auto seqAfter = compat::RetainedString::fromStaticLiteral("]");
    inline static const auto keySeparator = compat::RetainedString::fromStaticLiteral(":");
    inline static const auto seqSeparator = compat::RetainedString::fromStaticLiteral(", ");

    FormatSpec() = default;
    explicit FormatSpec(compat::RetainedStringSlice text) : trailing(std::move(text)) {}
    explicit FormatSpec(compat::RetainedString text) : trailing(std::move(text)) {}
    FormatSpec(const FormatSpec& other) { *this = other; }
    FormatSpec(FormatSpec&& other) noexcept { *this = other; }
    FormatSpec& operator=(const FormatSpec& other) noexcept {
        if (this == &other) return *this;
        width = other.width; precision = other.precision; separators = other.separators;
        dynamicSeparatorChar = other.dynamicSeparatorChar; separatorChar = other.separatorChar;
        spec = other.spec; indexStart = other.indexStart; indexEnd = other.indexEnd;
        allFlags = other.allFlags; nested = other.nested; sep = other.sep; trailing = other.trailing;
        return *this;
    }
    FormatSpec& operator=(FormatSpec&& other) noexcept { return *this = static_cast<const FormatSpec&>(other); }
    bool operator==(const FormatSpec& other) const noexcept {
        return width == other.width && precision == other.precision && separators == other.separators &&
            dynamicSeparatorChar == other.dynamicSeparatorChar && separatorChar == other.separatorChar &&
            spec == other.spec && indexStart == other.indexStart && indexEnd == other.indexEnd &&
            allFlags == other.allFlags && nested == other.nested && sep == other.sep && trailing == other.trailing;
    }
    int separatorCharPos() const noexcept { return dynamicSeparatorChar ? DYNAMIC : UNSPECIFIED; }
    void separatorCharPos(int value) noexcept { dynamicSeparatorChar = value == DYNAMIC; }

    template<class Writer> bool writeUpToNextSpec(Writer& writer) {
        if (trailing.empty()) return false;
        for (std::size_t i = 0; i < trailing.size(); ++i) {
            if (trailing.view()[i] != '%') continue;
            ngTypedFormatPut(writer, trailing.slice(0, i));
            trailing = trailing.slice(i, trailing.size());
            checkTerminated();
            trailing = trailing.slice(1, trailing.size());
            if (trailing.view()[0] != '%') { fillUp(); return true; }
            i = 0;
        }
        ngTypedFormatPut(writer, trailing);
        trailing.reset();
        return false;
    }
    compat::RetainedString getCurFmtStr() const;
    compat::RetainedString toString() const;
    template<class Writer> void toString(Writer& writer) const {
        // Each chunk is emitted before reading the next public field, as Source.
        writeDescription([&](compat::RetainedStringSlice text) { ngTypedFormatPut(writer, text); },
            [&](char value) { ngTypedFormatPut(writer, value); });
    }
private:
    void checkTerminated() const;
    void fillUp();
    template<class Sink, class Character>
    void writeDescription(const Sink& sink, const Character& character) const {
        const auto literal = [&]<std::size_t N>(const char (&text)[N]) {
            sink(compat::RetainedString::fromStaticLiteral(text));
        };
        const auto value = [&](auto field) {
            ngFormatDefaultValueTo(sink, field);
        };
        literal("address = "); value(const_cast<void*>(static_cast<const void*>(this)));
        literal("\nwidth = "); value(width);
        literal("\nprecision = "); value(precision);
        literal("\nspec = "); value(spec);
        literal("\nindexStart = "); value(indexStart);
        literal("\nindexEnd = "); value(indexEnd);
        literal("\nflDash = "); value(static_cast<bool>(flDash));
        literal("\nflZero = "); value(static_cast<bool>(flZero));
        literal("\nflSpace = "); value(static_cast<bool>(flSpace));
        literal("\nflPlus = "); value(static_cast<bool>(flPlus));
        literal("\nflEqual = "); value(static_cast<bool>(flEqual));
        literal("\nflHash = "); value(static_cast<bool>(flHash));
        literal("\nflSeparator = "); value(static_cast<bool>(flSeparator));
        literal("\nnested = "); value(nested);
        literal("\ntrailing = "); value(trailing);
        character('\n');
    }
};
using FormatSpecChar = FormatSpec;
FormatSpec ngSingleFormatSpec(compat::RetainedStringSlice text);

namespace detail {
template<class Writer> void putFormatCodePoint(Writer& writer, char32_t value) {
    using Output = std::remove_cvref_t<Writer>;
    if constexpr (requires {
        static_cast<decltype(writer.put(value)) (Output::*)(char32_t)>(&Output::put);
    }) {
        const auto method = static_cast<decltype(writer.put(value)) (Output::*)(char32_t)>(&Output::put);
        std::invoke(method, writer, value);
    } else if constexpr (requires {
        static_cast<decltype(writer.put(value)) (Output::*)(char32_t) const>(&Output::put);
    }) {
        const auto method = static_cast<decltype(writer.put(value)) (Output::*)(char32_t) const>(&Output::put);
        std::invoke(method, writer, value);
    } else if constexpr (requires { writer.template put<char32_t>(value); }) {
        writer.template put<char32_t>(value);
    } else if constexpr (requires { writer.template operator()<char32_t>(value); }) {
        writer.template operator()<char32_t>(value);
    } else {
        ngTypedFormatPut(writer, compat::RetainedStringSlice(
            compat::RetainedString(formatSeparatorUtf8(value))));
    }
}
} // namespace detail

enum class FormatPrecisionType { None, Integer, FractionalDigits, AllDigits };

namespace detail {
inline std::int32_t formatAddInt32(std::int32_t value, std::uint32_t addend) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + addend);
}
}

// The Source writeAligned body, including the flag reads after each put. The
// caller supplies the real components rather than a completed formatted field.
template<class Writer, class Suffix> void ngWriteAlignedFormatWithSuffixTo(Writer& writer,
    compat::RetainedStringSlice prefix, compat::RetainedStringSlice grouped,
    compat::RetainedStringSlice fracts, Suffix suffix,
    const FormatSpec& spec, FormatPrecisionType precision = FormatPrecisionType::None) {
    if (precision == FormatPrecisionType::Integer && spec.precision == FormatSpec::UNSPECIFIED)
        precision = FormatPrecisionType::None;
    std::int64_t prefixWidth = 0, suffixWidth = 0;
    const auto groupedWidth = static_cast<std::int64_t>(grouped.size());
    auto fractsWidth = static_cast<std::int64_t>(fracts.size());
    if (spec.width > 0) {
        prefixWidth = static_cast<std::int64_t>(detail::formatDisplayWidth(prefix.view()));
        if constexpr (requires { suffix.displayWidth(); }) suffixWidth = suffix.displayWidth();
        else suffixWidth = static_cast<std::int64_t>(detail::formatDisplayWidth(suffix.view()));
    }
    const bool grouping = spec.flSeparator && groupedWidth > 0 && spec.separators > 0 &&
        spec.separators != FormatSpec::UNSPECIFIED;
    auto front = grouping ? (groupedWidth - 1) % spec.separators + 1 : std::int64_t{0};
    const auto separatorCount = grouping ? (groupedWidth - 1) / spec.separators : std::int64_t{0};
    std::int64_t trailingZeros = 0;
    if (precision == FormatPrecisionType::FractionalDigits)
        trailingZeros = spec.precision - (fractsWidth - 1);
    if (precision == FormatPrecisionType::AllDigits && spec.flHash) {
        if (grouped.view() != "0") trailingZeros = spec.precision - (fractsWidth - 1) - groupedWidth;
        else {
            trailingZeros = spec.precision - fractsWidth;
            for (std::size_t i = 0; i < fracts.size(); ++i) {
                if (fracts.view()[i] != '0' && fracts.view()[i] != '.') {
                    trailingZeros = spec.precision - static_cast<std::int64_t>(fracts.size() - i);
                    break;
                }
            }
        }
    }
    const bool noDot = fracts.view() == "." && trailingZeros == 0 && !spec.flHash;
    if (noDot) fractsWidth = 0;
    const auto width = prefixWidth + separatorCount + groupedWidth + fractsWidth + trailingZeros + suffixWidth;
    auto delta = static_cast<std::int64_t>(spec.width) - width;
    std::int64_t pregrouped = 0;
    if (precision == FormatPrecisionType::Integer && groupedWidth < spec.precision) {
        pregrouped = spec.precision - groupedWidth;
        delta -= pregrouped;
        if (grouping) {
            front = ((front - 1) + pregrouped) % spec.separators + 1;
            delta -= detail::formatAddInt32(spec.precision, UINT32_MAX) / spec.separators - separatorCount;
        }
    }
    const auto padding = [&](std::int64_t count, char value) {
        for (std::int64_t i = 0; i < count; ++i) ngTypedFormatPut(writer, value);
    };
    if ((!spec.flZero || precision == FormatPrecisionType::Integer) && delta > 0) {
        if (spec.flEqual) padding(delta / 2 + (delta % 2 == 1 && !spec.flDash ? 1 : 0), ' ');
        else if (!spec.flDash) padding(delta, ' ');
    }
    ngTypedFormatPut(writer, prefix);
    if (spec.flZero && precision != FormatPrecisionType::Integer && !spec.flDash && delta > 0) {
        if (grouping) {
            auto zeroFront = (delta + front - 1) % detail::formatAddInt32(spec.separators, 1) + 1;
            const auto zeroSeparators = (delta + front - 1) / detail::formatAddInt32(spec.separators, 1);
            delta -= zeroSeparators;
            if (zeroFront > spec.separators) zeroFront = 1;
            for (std::int64_t i = 0; i < delta; ++i) {
                if (zeroFront == 0) {
                    detail::putFormatCodePoint(writer, spec.separatorChar);
                    zeroFront = spec.separators;
                }
                --zeroFront;
                ngTypedFormatPut(writer, '0');
            }
            if (front == spec.separators) detail::putFormatCodePoint(writer, spec.separatorChar);
        } else padding(delta, '0');
    }
    if (grouping) {
        const auto length = pregrouped + static_cast<std::int64_t>(grouped.size());
        for (std::int64_t i = 0; i < length; ++i) {
            if (front == 0) {
                detail::putFormatCodePoint(writer, spec.separatorChar);
                front = spec.separators;
            }
            --front;
            ngTypedFormatPut(writer, i < pregrouped ? '0' : grouped.view()[static_cast<std::size_t>(i - pregrouped)]);
        }
    } else {
        padding(pregrouped, '0');
        ngTypedFormatPut(writer, grouped);
    }
    if (!noDot) ngTypedFormatPut(writer, fracts);
    padding(trailingZeros, '0');
    if constexpr (requires { suffix.putTo(writer); }) suffix.putTo(writer);
    else ngTypedFormatPut(writer, suffix);
    if (delta > 0) {
        if (spec.flEqual) padding(delta / 2 + (delta % 2 == 1 && spec.flDash ? 1 : 0), ' ');
        else if (spec.flDash) padding(delta, ' ');
    }
}

template<class Writer> void ngWriteAlignedFormatTo(Writer& writer,
    compat::RetainedStringSlice prefix, compat::RetainedStringSlice grouped,
    compat::RetainedStringSlice fracts, compat::RetainedStringSlice suffix,
    const FormatSpec& spec, FormatPrecisionType precision = FormatPrecisionType::None) {
    ngWriteAlignedFormatWithSuffixTo(writer, prefix, grouped, fracts, std::move(suffix), spec, precision);
}

// Real character writer used for the Source custom-put-writer overloads.
class TypedFormatWriter {
    std::string& output_;
    template<class Append> void appendOwned(Append&& append) {
        try { std::forward<Append>(append)(); }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
public:
    explicit TypedFormatWriter(std::string& output) : output_(output) {}
    TypedFormatWriter(const TypedFormatWriter&) = delete;
    void put(char value) { appendOwned([&] { output_.push_back(value); }); }
    void put(char8_t value) { put(static_cast<char>(value)); }
    template<class Char> requires (std::same_as<Char, char16_t> || std::same_as<Char, char32_t> ||
        std::same_as<Char, wchar_t>)
    void put(Char value) {
        const auto encoded = detail::formatSeparatorUtf8(static_cast<char32_t>(value));
        appendOwned([&] { output_.append(encoded); });
    }
    void put(std::string_view value) { appendOwned([&] { output_.append(value); }); }
    void put(const compat::RetainedString& value) { put(value.view()); }
    void put(const compat::RetainedStringSlice& value) { put(value.view()); }
    template<std::size_t N> void put(const char (&value)[N]) { put(std::string_view(value, N - 1)); }
    template<class Char> requires (!std::same_as<Char, char>)
    void put(std::basic_string_view<Char> value) {
        if constexpr (sizeof(Char) == 1) {
            if (!value.empty()) appendOwned([&] {
                output_.append(reinterpret_cast<const char*>(value.data()), value.size());
            });
        } else if constexpr (sizeof(Char) == 4) {
            for (auto scalar : value) put(scalar);
        } else {
            compat::phobos::detail::UnicodeCursor<std::span<const Char>, Char> cursor{
                std::span<const Char>(value.data(), value.size())};
            while (!cursor.empty()) { put(cursor.front()); cursor.popFront(); }
        }
    }
    template<class Char> requires (!std::same_as<Char, char>)
    void put(const std::basic_string<Char>& value) { put(std::basic_string_view<Char>(value)); }
};
template<class Text> compat::RetainedStringSlice ngFormatStringResult(Text&& text) {
    using Value = std::remove_cvref_t<Text>;
    if constexpr (std::derived_from<Value, compat::RetainedStringSlice>) return text;
    else if constexpr (std::same_as<Value, compat::RetainedString>) return std::forward<Text>(text);
    else if constexpr (requires { text.retainedString(); }) return text.retainedString();
    else if constexpr (std::constructible_from<compat::RetainedString, Text>)
        return compat::RetainedString(std::forward<Text>(text));
    else {
        std::string output;
        TypedFormatWriter writer(output);
        writer.put(text);
        return compat::RetainedString(std::move(output));
    }
}
using RetainedFormatSink = compat::SharedDelegate<void(compat::RetainedStringSlice)>;
FormatSpec ngPublicFormatSpec(const detail::ParsedTypedFormat& spec);
detail::ParsedTypedFormat ngParsedFormatSpec(const FormatSpec& spec);
} // namespace nijigenerate::core::translation
