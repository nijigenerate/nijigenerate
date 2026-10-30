#pragma once

#include "crashdump.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <functional>
#include <new>
#include <stdexcept>
#include <climits>
#include <concepts>
#include <cstdint>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::utils::crashdump {
namespace detail {
// Call only for passive byte storage, never around a formatter, user toString,
// state conversion, or report callback.
template<class Allocate> decltype(auto) allocateCrashText(Allocate&& allocate,
        std::source_location location = std::source_location::current()) {
    try { return std::forward<Allocate>(allocate)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(location);
    }
    catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(location);
    }
}
template<class T> struct CrashReadOnlyCharSlice : std::false_type {};
template<class T> struct CrashReadOnlyCharSlice<compat::ConstSlice<T>>
    : std::bool_constant<std::same_as<std::remove_cv_t<T>, char> ||
        std::same_as<std::remove_cv_t<T>, char8_t>> {};

template<class T> struct CrashStaticArray : std::false_type {};
template<class T, std::size_t N> struct CrashStaticArray<std::array<T, N>> : std::true_type {};
template<class T> struct CrashSharedClass : std::false_type {};
template<class T> struct CrashSharedClass<std::shared_ptr<T>> : std::true_type {};

// C++ function parameters adjust raw arrays to pointers even when their
// template argument names the complete array type. Materialize the actual
// static-array value in the existing owning carrier before that adjustment.
template<class T> struct CrashSourceFrame { using Type = T; };
template<class T, std::size_t N> struct CrashSourceFrame<T[N]> {
    using Type = std::array<typename CrashSourceFrame<T>::Type, N>;
};
template<class T> using CrashSourceFrameType = typename CrashSourceFrame<T>::Type;

template<class T> decltype(auto) crashSourceFrameValue(T& value);

template<class T, std::size_t N, std::size_t... I>
CrashSourceFrameType<T[N]> crashSourceArrayValue(T (&value)[N], std::index_sequence<I...>) {
    return {crashSourceFrameValue(value[I])...};
}

template<class T> decltype(auto) crashSourceFrameValue(T& value) {
    if constexpr (std::is_array_v<T>)
        return crashSourceArrayValue(value, std::make_index_sequence<std::extent_v<T>>{});
    else return (value);
}
template<class T> constexpr bool crashCharacter =
    std::same_as<T, char> || std::same_as<T, char8_t> || std::same_as<T, char16_t> ||
    std::same_as<T, char32_t> || std::same_as<T, wchar_t>;

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const T& value);
template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const compat::ConstSlice<T>& value);

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const compat::Slice<T>& value);

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(
    const nicxlive::core::common::SharedArray<T>& value);

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const std::shared_ptr<T>& value);
template<class T, std::size_t N>
core::translation::TypedFormatArgument crashStateFormatArgument(const std::array<T, N>& value);

// The actual text appender writes into this conversion frame. A noncopyable
// writer also preserves hasToString's custom-put-writer selection probe.
class CrashTextWriter {
    std::string& text_;
public:
    explicit CrashTextWriter(std::string& text) : text_(text) {}
    CrashTextWriter(const CrashTextWriter&) = delete;
    void put(char value) { allocateCrashText([&] { text_.push_back(value); }); }
    void put(char8_t value) { put(static_cast<char>(value)); }
    template<class Char> requires (std::same_as<Char, char16_t> ||
        std::same_as<Char, char32_t> || std::same_as<Char, wchar_t>)
    void put(Char value) {
        const auto bytes = allocateCrashText([&] {
            return core::translation::detail::formatSeparatorUtf8(static_cast<char32_t>(value));
        });
        put(bytes);
    }
    void put(std::string_view value) { allocateCrashText([&] { text_.append(value); }); }
    void put(const std::string& value) { put(std::string_view(value)); }
    template<class Char> requires (crashCharacter<Char> && !std::same_as<Char, char>)
    void put(std::basic_string_view<Char> value) { putCharacters<Char>(value); }
    template<class Char> requires (crashCharacter<Char> && !std::same_as<Char, char>)
    void put(const std::basic_string<Char>& value) { putCharacters<Char>(value); }
    template<std::size_t N> void put(const char (&value)[N]) { put(std::string_view(value, N - 1)); }
    void put(const compat::RetainedString& value) { put(value.view()); }
    void put(const compat::RetainedStringSlice& value) { put(value.view()); }
    template<class Char> requires crashCharacter<Char>
    void put(compat::Slice<Char> value) { putCharacters<Char>(value); }
    template<class Char> requires crashCharacter<Char>
    void put(compat::ConstSlice<Char> value) { putCharacters<Char>(value); }
    template<class Char> requires crashCharacter<Char>
    void put(nicxlive::core::common::SharedArray<Char> value) { putCharacters<Char>(value); }

private:
    template<class Char, class Range> void putCharacters(const Range& value) {
        // Appender's same-width character path copies code units, including
        // embedded zeroes and malformed UTF-8. No terminator scan or decoding.
        if constexpr (sizeof(Char) == 1) {
            if (!value.empty())
                put(std::string_view(reinterpret_cast<const char*>(value.data()), value.size()));
        } else {
            // Wider ranges decode through the canonical cursor, then use the
            // same scalar encoder as Appender.put. A later failure preserves
            // characters that were already appended to the live output.
            compat::phobos::detail::UnicodeCursor<std::span<const Char>, Char> cursor{
                std::span<const Char>(value.data(), value.size())};
            while (!cursor.empty()) {
                put(cursor.front());
                cursor.popFront();
            }
        }
    }
};

template<class T> std::string crashFormatValue(T& value) {
    using core::translation::TypedFormatSink;
    std::string result;
    CrashTextWriter writer(result);
    const core::translation::FormatSpec spec;
    TypedFormatSink sink = allocateCrashText([&] {
        return TypedFormatSink([&](std::string_view bytes) { writer.put(bytes); });
    });
    // The shared provider owns Source custom-writer/sink selection, including
    // retained string delegates and the real public FormatSpec. Keep this
    // conversion frame and live writer; do not copy the state into an adapter.
    if constexpr (core::translation::HasCustomFormat<T> ||
            requires { value.toString(writer, spec); } || requires { value.toString(writer); } ||
            core::translation::HasAggregateFormatMetadata<std::remove_cvref_t<T>>)
        core::translation::ngFormatValueTo(writer, value, spec);
    // Preserve the older explicit native string_view-only intake separately.
    else if constexpr (requires { value.toString(sink, std::string_view("%s")); }) {
        const auto format = spec.getCurFmtStr();
        value.toString(sink, format.view());
    }
    else if constexpr (core::translation::HasFormatObjectTraits<std::remove_cvref_t<T>>) {
        // Invoke the defining value formatter on this actual conversion local,
        // rather than copying it into the generic owning argument adapter.
        writer.put(core::translation::FormatObjectTraits<std::remove_cvref_t<T>>::toString(value));
    }
    else {
        const std::array<core::translation::TypedFormatArgument, 1> args{
            crashStateFormatArgument(value)};
        result = core::translation::ngFormatTyped("%s", args);
    }
    return result;
}

template<class T> std::string crashFormatValueFrame(T value) {
    // Scalar and enum formatValueImpl overloads retain their value frame.
    // Aggregate/custom auto-ref overloads bypass this function below.
    return crashFormatValue(value);
}

template<class T> core::translation::TypedFormatArgument crashStateFormatArgument(const T& value) {
    if constexpr (std::is_pointer_v<T>)
        return core::translation::FormatPointer{reinterpret_cast<std::uintptr_t>(value)};
    else return core::translation::ngFormatArgument(value);
}

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const std::shared_ptr<T>& value) {
    // Array element formatting enters through this same module. A class value
    // retains its actual handle, never a copied pointee or a borrowed receiver.
    // genCrashDump's text conversion uses the default %s spec throughout.
    core::translation::FormatObject result;
    const auto name = core::translation::FormatSourceTypeName<T>::get();
    if (!name.empty()) result.sourceTypeName = allocateCrashText([&] { return compat::RetainedString(name); });
    allocateCrashText([&] { result.toString = [owner = value] {
        if (!owner) return compat::RetainedString::fromStaticLiteral("null");
        auto bytes = crashFormatValue(*owner);
        return allocateCrashText([&] { return compat::RetainedString(std::move(bytes)); });
    }; });
    return result;
}

template<class Array>
core::translation::TypedFormatArgument crashRetainedArrayArgument(Array values) {
    using Element = std::remove_cvref_t<decltype(values[0])>;
    auto range = std::allocate_shared<core::translation::FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<core::translation::FormatRange>{});
    range->length = values.size();
    range->sourceTypeName = core::translation::FormatSourceTypeName<Array>::get();
    if constexpr (!core::translation::FormatArrayElement<Element>)
        range->characterBits = static_cast<unsigned>(sizeof(Element) * CHAR_BIT);
    // Keep the real descriptor and read each live cell only when requested.
    // Recurse through this module so dynamic arrays can contain static arrays
    // without requiring a second canonical formatter or eager element copies.
    allocateCrashText([&] { range->read = [values = std::move(values)](std::size_t index) {
        return crashStateFormatArgument(values[index]);
    }; });
    return range;
}

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(const compat::Slice<T>& value) {
    return crashRetainedArrayArgument(value);
}

template<class T>
core::translation::TypedFormatArgument crashStateFormatArgument(
    const nicxlive::core::common::SharedArray<T>& value) {
    return crashRetainedArrayArgument(value);
}

template<class T> core::translation::TypedFormatArgument crashStateFormatArgument(
    const compat::ConstSlice<T>& value) {
    using Element = std::remove_cv_t<T>;
    auto range = std::allocate_shared<core::translation::FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<core::translation::FormatRange>{});
    range->length = value.size();
    const auto elementName = core::translation::FormatSourceTypeName<Element>::get();
    if (!elementName.empty()) allocateCrashText([&] { range->sourceTypeName = "const(" + elementName + ")[]"; });
    if constexpr (!core::translation::FormatArrayElement<Element>)
        range->characterBits = static_cast<unsigned>(sizeof(Element) * CHAR_BIT);
    // The real readonly descriptor retains the source cells; the formatter
    // reads them in order instead of consuming an eager copied argument array.
    allocateCrashText([&] { range->read = [values = value](std::size_t index) {
        return crashStateFormatArgument(values[index]);
    }; });
    return range;
}

template<class T, std::size_t N>
core::translation::TypedFormatArgument crashStateFormatArgument(const std::array<T, N>& value) {
    using Element = std::remove_cv_t<T>;
    auto range = std::allocate_shared<core::translation::FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<core::translation::FormatRange>{});
    range->length = N;
    const auto elementName = core::translation::FormatSourceTypeName<Element>::get();
    if (!elementName.empty()) allocateCrashText([&] { range->sourceTypeName = elementName + "[]"; });
    if constexpr (!core::translation::FormatArrayElement<Element>)
        range->characterBits = static_cast<unsigned>(sizeof(Element) * CHAR_BIT);
    // Static-array elements remain in the live conversion frame, including
    // nested arrays. Only the immediate formatter consumes these readers.
    allocateCrashText([&] {
        range->read = [&value](std::size_t index) { return crashStateFormatArgument(value[index]); };
    });
    return range;
}

template<class T> std::string crashFormatText(T value) {
    if constexpr (std::same_as<std::remove_cvref_t<T>, std::nullptr_t>)
        return allocateCrashText([] { return std::string("null"); });
    else if constexpr (std::is_pointer_v<T> &&
            crashCharacter<std::remove_cv_t<std::remove_pointer_t<T>>>) {
        using Char = std::remove_cv_t<std::remove_pointer_t<T>>;
        if (!value) return {};
        const auto length = std::char_traits<Char>::length(value);
        if (!length) return {};
        // std.conv copies the terminated pointer range before converting it.
        const auto characters = allocateCrashText([&] {
            return std::allocate_shared<std::basic_string<Char>>(
                nicxlive::core::d_runtime::SourceAllocator<std::basic_string<Char>>{}, value, length);
        });
        if constexpr (sizeof(Char) == 1)
            return allocateCrashText([&] {
                return std::string(reinterpret_cast<const char*>(characters->data()), characters->size());
            });
        else {
            auto range = std::allocate_shared<core::translation::FormatRange>(
                nicxlive::core::d_runtime::SourceAllocator<core::translation::FormatRange>{});
            range->length = characters->size();
            range->characterBits = static_cast<unsigned>(sizeof(Char) * CHAR_BIT);
            allocateCrashText([&] { range->read = [characters](std::size_t index) {
                return core::translation::ngFormatArgument((*characters)[index]);
            }; });
            const std::array<core::translation::TypedFormatArgument, 1> args{range};
            return core::translation::ngFormatTyped("%s", args);
        }
    }
    else if constexpr (core::translation::HasCustomFormatFor<T, CrashTextWriter> ||
            core::translation::HasFormatObjectTraits<std::remove_cvref_t<T>> ||
            core::translation::HasAggregateFormatMetadata<std::remove_cvref_t<T>> ||
            requires(T& object, core::translation::TypedFormatSink& sink) {
                object.toString(sink, std::string_view("%s"));
            })
        return crashFormatValue(value);
    else return crashFormatValueFrame<T>(value);
}

// std.conv.text -> textImpl -> to!string -> toImpl -> toStr each receives this
// general branch by value. Its aggregate/custom formatValueImpl then forwards
// the toStr local by auto ref; scalar/enum overloads keep their value frame.
template<class T> std::string crashToStringImpl(T value) { return crashFormatText<T>(value); }
template<class T> std::string crashToString(T value) { return crashToStringImpl<T>(value); }
template<class T> std::string crashTextImpl(T value) { return crashToString<T>(value); }

template<class Array> std::string crashStaticTextImpl(Array value) {
    // to!string's static-array overload takes a reference, then slices that
    // local array. This reader is consumed synchronously before value dies.
    const std::array<core::translation::TypedFormatArgument, 1> args{crashStateFormatArgument(value)};
    return core::translation::ngFormatTyped("%s", args);
}

template<class T> std::string crashStateText(T value) {
    if constexpr (CrashStaticArray<std::remove_cv_t<T>>::value) return crashStaticTextImpl<T>(value);
    else return crashTextImpl<T>(value);
}

template<class T>
std::string crashStateText(std::shared_ptr<T> value) {
    // A source class argument copies its actual owning handle, not the object.
    // Retain that handle while virtual toString can release external references.
    if (!value) return allocateCrashText([] { return std::string("null"); });
    return crashFormatValue(*value);
}

template<class T> compat::RetainedString serializeCrashDumpState(T& value) {
    using V = std::remove_cvref_t<T>;
    if constexpr (std::same_as<V, std::nullptr_t>)
        return compat::RetainedString::fromStaticLiteral("null");
    else {
        // Explicit T keeps source const qualifiers in each value conversion
        // frame; ordinary C++ by-value deduction would silently remove them.
        auto text = [&] {
            if constexpr (CrashSharedClass<std::remove_cv_t<T>>::value)
                return crashStateText(value);
            else return crashStateText<T>(value);
        }();
        if constexpr (std::same_as<V, std::string> || std::same_as<V, std::string_view> ||
            std::same_as<V, compat::RetainedString> || std::same_as<V, compat::RetainedStringSlice> ||
            CrashReadOnlyCharSlice<V>::value ||
            (std::is_array_v<V> && std::same_as<std::remove_cv_t<std::remove_extent_t<V>>, char>)) {
            return allocateCrashText([&] {
                std::string quoted = "\"";
                for (const char ch : text) {
                    if (ch == '\\' || ch == '"') quoted += '\\';
                    quoted += ch;
                }
                quoted += '"';
                return compat::RetainedString(std::move(quoted));
            });
        }
        return allocateCrashText([&] { return compat::RetainedString(std::move(text)); });
    }
}
template<class Describe, class... State>
compat::RetainedString genCrashDumpText(Describe&& describe, State&... state) {
    compat::Slice<compat::RetainedStringSlice> arguments;
    // Match the source's ordered string[] appends, retaining each completed
    // serialization before evaluating the following state conversion.
    (arguments.append(compat::RetainedStringSlice(detail::serializeCrashDumpState(state))), ...);
    std::string report = allocateCrashText([] { return std::string("=== Args State ===\n"); });
    if (!arguments.empty()) {
        std::size_t length = 0;
        for (const auto& argument : arguments) length += argument.size();
        length += (arguments.size() - 1) * 2;
        std::string joined = allocateCrashText([&] { return std::string(length, '\0'); });
        std::size_t position = 0;
        const auto put = [&](std::string_view value, std::size_t sourceLine) {
            for (const auto byte : value) {
                if (position >= joined.size())
                    throw compat::ngStaticError<compat::ArrayIndexError>(position, joined.size(),
                        compat::RetainedString::fromStaticLiteral("std/array.d"), sourceLine);
                joined[position++] = byte;
            }
        };
        put(arguments[0].view(), 2258);
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            put(",\n", 2263);
            put(arguments[i].view(), 2265);
        }
        allocateCrashText([&] { report += joined; });
    }
    allocateCrashText([&] { report += "\n\n=== Exception ===\n"; });
    const auto description = std::forward<Describe>(describe)();
    allocateCrashText([&] { report += description.view(); });
    return allocateCrashText([&] { return compat::RetainedString(std::move(report)); });
}
} // namespace detail

// The source report text is common to Windows and POSIX. Keep its definition
// independent of each platform's file output, fallback and notification flow.
template<class... State>
compat::RetainedString genCrashDump(const compat::Throwable& throwable, State&&... incoming) {
    // The native entry forwards into one source by-value parameter frame,
    // retaining top-level const rather than erasing it during deduction.
    return [&]<class... Value>(Value... state) {
        return detail::genCrashDumpText([&] { return throwable.toString(); }, state...);
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}

template<class... State>
compat::RetainedString genCrashDump(std::shared_ptr<const compat::Throwable> throwable, State&&... incoming) {
    // Source state conversions precede the nullable Throwable dereference. Keep
    // its real owner alive even when a state conversion changes external owners.
    return [&]<class... Value>(Value... state) {
        return detail::genCrashDumpText([throwable = std::move(throwable)] {
            return compat::ngRequireObject(throwable)->toString();
        }, state...);
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}
} // namespace nijigenerate::utils::crashdump
