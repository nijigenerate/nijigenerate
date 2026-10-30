#pragma once
// Phobos formatting adaptations: Copyright The D Language Foundation 2000-2013.
// BSL-1.0; full notice retained in typed_format_layout.hpp.

#include "typed_format_decimal.hpp"
#include "typed_format_layout.hpp"
#include "typed_format_hex.hpp"
#include "typed_format_enum.hpp"
#include "typed_format_quote.hpp"
#include "typed_format_grapheme.hpp"
#include "typed_format_characters.hpp"
#include "typed_format_error.hpp"
#include "typed_format_spec.hpp"
#include "typed_format_float_writer.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/source_fixed_array.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <span>
#include <variant>
#include <tuple>
#include <vector>

namespace nicxlive::core::common { template<class T> class SharedArray; class NullableString; }

namespace nijigenerate::core::translation {
using TypedFormatSink = std::function<void(std::string_view)>;

struct FormatInteger {
    std::uint64_t raw = 0;
    unsigned bits = 64;
    bool negative = false;
    bool signedType = false;
};
struct FormatCharacter { char32_t value; unsigned bits = 32; };
struct FormatPointer {
    std::uintptr_t address;
    std::string_view sourceTypeName = "void*";
};
struct FormatEnum {
    std::variant<FormatInteger, bool, FormatCharacter, compat::RetainedStringSlice> base;
    compat::RetainedStringSlice typeName;
    std::optional<compat::RetainedStringSlice> memberName;
};
struct FormatRange;
struct FormatAggregate;
class FormatWriterRef;
struct FormatObject {
    compat::RetainedStringSlice sourceTypeName;
    compat::SharedDelegate<compat::RetainedString()> toString;
    compat::SharedDelegate<compat::RetainedString(const FormatSpec&)> format;
    bool acceptsFormatSpec = false;
    compat::SharedDelegate<void(FormatWriterRef&, const FormatSpec&)> write;
};
namespace detail {
template<class Unit> struct FormatCharacterRangeSuffix;
inline std::string copyFormatTypeName(std::string_view name);
template<class... Extended> using TypedFormatStorage = std::variant<compat::RetainedStringSlice,
    FormatInteger, float, double, bool,
    FormatCharacter, FormatEnum, FormatObject, std::shared_ptr<FormatRange>, FormatPointer,
    std::shared_ptr<FormatAggregate>, Extended...>;
}
using TypedFormatArgument = std::conditional_t<detail::formatExtendedRealEnabled,
    detail::TypedFormatStorage<long double>, detail::TypedFormatStorage<>>;
struct FormatAggregateField {
    std::string_view name;
    std::size_t offset;
    bool hiddenContext = false;
};
struct FormatAggregate {
    std::string sourceTypeName;
    std::vector<FormatAggregateField> fields;
    compat::SharedDelegate<TypedFormatArgument(std::size_t)> read;
    bool isUnion = false;
};
namespace detail {
template<class Build> decltype(auto) allocateAggregateText(Build&& build) {
    try { return std::forward<Build>(build)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
// Producers live beside actual declarations. Missing arbitrary Source metadata
// is not guessed from RTTI, object bytes, a field-count limit or a type whitelist.
template<class T> struct FormatAggregateTraits;
template<class T> struct FormatObjectTraits;
namespace detail {
// Used only by an actual declaration's corresponding no-argument Object-format
// trait. This is not a blanket class/reference or overload-discovery fallback.
template<class Reference> struct SourceNoArgObjectFormat {
    static compat::RetainedString typeName() {
        return nicxlive::core::d_runtime::ngSourceTypeSpelling<Reference>(
            nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
    }
    static compat::RetainedString toString(const Reference& value) {
        if (!value) return compat::RetainedString::fromStaticLiteral("null");
        return value->toString();
    }
};
} // namespace detail
template<class T> inline constexpr bool formatUnionMetadata = [] {
    if constexpr (requires { FormatAggregateTraits<T>::isUnion; })
        return static_cast<bool>(FormatAggregateTraits<T>::isUnion);
    else return false;
}();
template<class T> concept HasAggregateFormatMetadata = requires { FormatAggregateTraits<T>::typeName; } &&
    (formatUnionMetadata<T> || requires { FormatAggregateTraits<T>::fields(); });
struct FormatRange {
    std::size_t length = 0;
    std::function<TypedFormatArgument(std::size_t)> read;
    unsigned characterBits = 0;
    std::string sourceTypeName;
    std::function<bool()> empty;
    std::function<void()> popFront;
    std::variant<std::monostate, std::string_view, std::u8string_view, std::u16string_view,
        std::u32string_view, std::wstring_view> characterView;
};
// A scoped reference to the actual output writer for retained custom values.
class FormatWriterRef {
    using CharacterView = std::variant<std::string_view, std::u8string_view, std::u16string_view,
        std::u32string_view, std::wstring_view>;
    // Keep function references separate from object pointers and restore the exact Writer before use.
    using WriterStorage = std::variant<const volatile void*, void(*)()>;
    WriterStorage writer_;
    void (*character_)(const WriterStorage&, char);
    void (*codePoint_)(const WriterStorage&, char32_t);
    void (*retained_)(const WriterStorage&, compat::RetainedStringSlice);
    void (*range_)(const WriterStorage&, CharacterView);
    template<class Writer> static WriterStorage storeWriter(Writer& writer) {
        if constexpr (std::is_function_v<Writer>)
            return WriterStorage(std::in_place_index<1>, reinterpret_cast<void(*)()>(std::addressof(writer)));
        else
            return WriterStorage(std::in_place_index<0>,
                static_cast<const volatile void*>(std::addressof(writer)));
    }
    template<class Writer> static Writer& writerFrom(const WriterStorage& storage) {
        if constexpr (std::is_function_v<Writer>)
            return *reinterpret_cast<Writer*>(std::get<1>(storage));
        else
            // Restore the original cv-qualified pointer; no unqualified Writer access occurs.
            return *static_cast<Writer*>(const_cast<void*>(std::get<0>(storage)));
    }
public:
    template<class Writer> requires (!std::same_as<std::remove_cvref_t<Writer>, FormatWriterRef>)
    explicit FormatWriterRef(Writer& writer)
        : writer_(storeWriter(writer)),
          character_(+[](const WriterStorage& storage, char value) {
              ngTypedFormatPut(writerFrom<Writer>(storage), value);
          }),
          codePoint_(+[](const WriterStorage& storage, char32_t value) {
              detail::putFormatCodePoint(writerFrom<Writer>(storage), value);
          }),
          retained_(+[](const WriterStorage& storage, compat::RetainedStringSlice value) {
              ngTypedFormatPut(writerFrom<Writer>(storage), value);
          }),
          range_(+[](const WriterStorage& storage, CharacterView value) {
              auto& writer = writerFrom<Writer>(storage);
              std::visit([&](const auto& view) {
                  using Unit = typename std::remove_cvref_t<decltype(view)>::value_type;
                  detail::FormatCharacterRangeSuffix<Unit>{view}.putTo(writer);
              }, value);
          }) {}
    void put(char value) { character_(writer_, value); }
    void put(char8_t value) { character_(writer_, static_cast<char>(value)); }
    void put(char32_t value) { codePoint_(writer_, value); }
    void put(char16_t value) { put(std::u16string_view(&value, 1)); }
    void put(wchar_t value) { put(std::wstring_view(&value, 1)); }
    void put(compat::RetainedStringSlice value) { retained_(writer_, std::move(value)); }
    void put(const compat::RetainedString& value) { retained_(writer_, value); }
    template<class Unit> void put(std::basic_string_view<Unit> value) { range_(writer_, CharacterView(value)); }
    template<class Unit> void put(const std::basic_string<Unit>& value) { put(std::basic_string_view<Unit>(value)); }
    template<class Unit, std::size_t N> void put(const Unit (&value)[N]) {
        put(std::basic_string_view<Unit>(value, N - 1));
    }
};

template<class T> struct FormatSourceTypeName {
    static std::string get() {
        if constexpr (std::same_as<T,bool>) return detail::copyFormatTypeName("bool");
        else if constexpr (std::same_as<T,char> || std::same_as<T,char8_t>)
            return detail::copyFormatTypeName("char");
        else if constexpr (std::same_as<T,char16_t> || (std::same_as<T,wchar_t> && sizeof(wchar_t)==2))
            return detail::copyFormatTypeName("wchar");
        else if constexpr (std::same_as<T,char32_t> || std::same_as<T,wchar_t>)
            return detail::copyFormatTypeName("dchar");
        else if constexpr (std::same_as<T,float>) return detail::copyFormatTypeName("float");
        else if constexpr (std::same_as<T,double>) return detail::copyFormatTypeName("double");
        else if constexpr (detail::FormatExtendedReal<T>) return detail::copyFormatTypeName("real");
        else if constexpr (std::same_as<T,void*>) return detail::copyFormatTypeName("void*");
        else if constexpr (std::same_as<T,const char*>) return detail::copyFormatTypeName("const(char)*");
        else if constexpr (std::integral<T>) {
            if constexpr (sizeof(T)==1) return detail::copyFormatTypeName(std::is_signed_v<T>?"byte":"ubyte");
            if constexpr (sizeof(T)==2) return detail::copyFormatTypeName(std::is_signed_v<T>?"short":"ushort");
            if constexpr (sizeof(T)==4) return detail::copyFormatTypeName(std::is_signed_v<T>?"int":"uint");
            return detail::copyFormatTypeName(std::is_signed_v<T>?"long":"ulong");
        } else if constexpr (std::same_as<T,compat::RetainedString> ||
            std::derived_from<T,compat::RetainedStringSlice> || std::same_as<T,std::string>)
            return detail::copyFormatTypeName("string");
        else if constexpr (HasNumericEnumFormatMetadata<T>)
            return detail::copyFormatTypeName(FormatNumericEnumTraits<T>::name);
        else if constexpr (requires { FormatEnumTraits<T>::name; })
            return detail::copyFormatTypeName(FormatEnumTraits<T>::name);
        else if constexpr (requires { FormatAggregateTraits<T>::typeName; })
            return detail::copyFormatTypeName(FormatAggregateTraits<T>::typeName);
        else if constexpr (requires {
            { FormatObjectTraits<T>::typeName() } -> std::same_as<compat::RetainedString>;
        }) {
            const auto name = FormatObjectTraits<T>::typeName();
            return detail::copyFormatTypeName(name.view());
        } else if constexpr (requires { FormatObjectTraits<T>::typeName; }) {
            using Name = decltype(FormatObjectTraits<T>::typeName);
            if constexpr (std::is_array_v<Name> &&
                std::same_as<std::remove_cv_t<std::remove_extent_t<Name>>, char>)
                return detail::copyFormatTypeName(FormatObjectTraits<T>::typeName);
            else return std::string(FormatObjectTraits<T>::typeName);
        } else return {}; // A source-specific adapter must supply actual metadata.
    }
};
template<class T> struct FormatSourceTypeName<compat::Slice<T>> {
    static std::string get() {
        const std::string element = FormatSourceTypeName<T>::get();
        try { return element.empty() ? element : element + "[]"; }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
};
template<class T, std::size_t N> struct FormatSourceTypeName<std::array<T, N>> {
    static std::string get() {
        const auto name = nicxlive::core::d_runtime::ngSourceTypeSpelling<std::array<T, N>>(
            nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
        return detail::copyFormatTypeName(name.view());
    }
};

template<> struct FormatSourceTypeName<FormatSpec> {
    static std::string get() { return detail::copyFormatTypeName("FormatSpec!char"); }
};
template<class T> struct FormatSourceTypeName<nicxlive::core::common::SharedArray<T>> {
    static std::string get() { return FormatSourceTypeName<compat::Slice<T>>::get(); }
};
template<class T> concept HasFormatObjectTraits = requires(const T& value) {
    { FormatObjectTraits<T>::toString(value) } -> std::same_as<compat::RetainedString>;
};
// The canonical object-format intake also covers Phobos's const-class fallback
// when Object.toString cannot be called on that qualified reference. The body
// still requires the genuine ClassInfo producer; no native name is invented.
template<class T> concept HasQualifiedClassFormat =
    std::is_const_v<T> && !std::is_volatile_v<T> && std::is_polymorphic_v<std::remove_const_t<T>>;
template<class T, class Writer> concept HasCustomFormatFor =
    requires(T& value, Writer& writer, const FormatSpec& spec) { value.toString(writer, spec); } ||
    requires(T& value, Writer& writer) { value.toString(writer); } ||
    requires(T& value, RetainedFormatSink& sink, const FormatSpec& spec) { value.toString(sink, spec); } ||
    requires(T& value, RetainedFormatSink& sink, const compat::RetainedString& text) { value.toString(sink, text); } ||
    requires(T& value, RetainedFormatSink& sink) { value.toString(sink); } ||
    requires(T& value, TypedFormatSink& sink, const FormatSpec& spec) { value.toString(sink, spec); } ||
    requires(T& value, TypedFormatSink& sink, const compat::RetainedString& text) { value.toString(sink, text); } ||
    requires(T& value, TypedFormatSink& sink) { value.toString(sink); } ||
    requires(T& value, Writer& writer) { writer.put(value.toString()); } ||
    HasQualifiedClassFormat<T>;
template<class T> concept HasCustomFormat = HasCustomFormatFor<T, TypedFormatWriter>;
template<class T> TypedFormatArgument ngCustomFormatArgument(T value);
template<class T> TypedFormatArgument ngAggregateFormatArgument(T value);
template<class T> requires HasCustomFormat<T>
TypedFormatArgument ngFormatArgument(T value) { return ngCustomFormatArgument(std::move(value)); }
template<class T> requires (!HasFormatObjectTraits<T> && !HasCustomFormat<T> && HasAggregateFormatMetadata<T>)
TypedFormatArgument ngFormatArgument(T value) { return ngAggregateFormatArgument(std::move(value)); }
// Visible before retained-array readers are defined, including nested arrays
// whose element adapters are supplied by their ordinary public headers.
template<class T> requires (!HasCustomFormat<T> && requires(const T& value) {
    { FormatObjectTraits<T>::toString(value) } -> std::same_as<compat::RetainedString>;
})
TypedFormatArgument ngFormatArgument(T value) {
    const auto name = [] {
        if constexpr (requires {
            { FormatObjectTraits<T>::typeName() } -> std::same_as<compat::RetainedString>;
        }) return FormatObjectTraits<T>::typeName();
        else return compat::RetainedString::fromStaticLiteral(FormatObjectTraits<T>::typeName);
    }();
    return FormatObject{
        compat::RetainedStringSlice(name),
        [value = std::move(value)] { return FormatObjectTraits<T>::toString(value); }};
}
template<class T> concept FormatArrayElement = !std::same_as<T, char> && !std::same_as<T, char8_t> &&
    !std::same_as<T, char16_t> && !std::same_as<T, char32_t> && !std::same_as<T, wchar_t>;
// Make both retained-array overloads visible for nested cross-carrier arrays.
// The runtime-specific definition stays in typed_format_shared_array.hpp.
template<class T> requires requires(const T& value) {
    FormatStringEnumTraits<T>::raw(value);
    FormatStringEnumTraits<T>::typeName();
    FormatStringEnumTraits<T>::memberName(value);
}
TypedFormatArgument ngFormatArgument(T value) {
    using Traits = FormatStringEnumTraits<T>;
    FormatEnum result;
    result.base = compat::RetainedStringSlice(Traits::raw(value));
    result.typeName = compat::RetainedStringSlice(
        compat::RetainedString::fromStaticView(Traits::typeName()));
    if (const auto name = Traits::memberName(value)) result.memberName = compat::RetainedStringSlice(
        compat::RetainedString::fromStaticView(*name));
    return result;
}

template<class T> TypedFormatArgument ngFormatArgument(compat::Slice<T> values);

// Borrowed readers are consumed synchronously while the actual writer frame
// remains alive. This producer adds no array or element value copy.
template<class Array> requires compat::SourceFixedArray<std::remove_cv_t<Array>>::value
TypedFormatArgument ngFormatFixedArrayReferenceArgument(Array& value, std::shared_ptr<void> owner = {});

template<class T> requires (HasCustomFormat<T> || HasAggregateFormatMetadata<std::remove_cv_t<T>>)
TypedFormatArgument ngFormatValueReferenceArgument(T& value, std::shared_ptr<void> owner = {});
template<class T>
TypedFormatArgument ngFormatArgument(nicxlive::core::common::SharedArray<T> values);

// The actual SDK descriptor adapter is defined in the existing node.cpp module.
TypedFormatArgument ngFormatArgument(const nicxlive::core::common::NullableString& value);

inline TypedFormatArgument ngFormatArgument(compat::RetainedStringSlice value) { return value; }
inline TypedFormatArgument ngFormatArgument(FormatFlag value) { return static_cast<bool>(value); }
inline TypedFormatArgument ngFormatArgument(compat::RetainedString value) {
    return compat::RetainedStringSlice(std::move(value));
}
inline TypedFormatArgument ngFormatArgument(const std::string& value) {
    try {
        return compat::RetainedStringSlice(compat::RetainedString(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline TypedFormatArgument ngFormatArgument(std::string&& value) {
    try {
        return compat::RetainedStringSlice(compat::RetainedString(std::move(value)));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline TypedFormatArgument ngFormatArgument(std::string_view value) {
    try {
        return compat::RetainedStringSlice(compat::RetainedString(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline TypedFormatArgument ngFormatArgument(void* value) {
    return FormatPointer{reinterpret_cast<std::uintptr_t>(value)};
}
template<class T> requires std::same_as<std::remove_cvref_t<T>, const char*>
TypedFormatArgument ngFormatArgument(T&& value) {
    return FormatPointer{reinterpret_cast<std::uintptr_t>(value), "const(char)*"};
}
template<std::size_t N> TypedFormatArgument ngFormatArgument(const char (&value)[N]) {
    static_assert(N > 0);
    return ngFormatArgument(std::string_view(value, N - 1));
}
template<class T> requires (detail::FormatFloating<T> || std::same_as<T, bool>)
TypedFormatArgument ngFormatArgument(T value) { return value; }
template<class T> requires (std::same_as<T, char> || std::same_as<T, char8_t> ||
    std::same_as<T, char16_t> || std::same_as<T, char32_t> || std::same_as<T, wchar_t>)
TypedFormatArgument ngFormatArgument(T value) {
    return FormatCharacter{static_cast<char32_t>(static_cast<std::make_unsigned_t<T>>(value)),
        static_cast<unsigned>(sizeof(T) * CHAR_BIT)};
}
template<std::integral T> requires (!std::same_as<T, bool> && !std::same_as<T, char> &&
    !std::same_as<T, wchar_t> && !std::same_as<T, char8_t> && !std::same_as<T, char16_t> &&
    !std::same_as<T, char32_t> && sizeof(T) <= sizeof(std::uint64_t))
TypedFormatArgument ngFormatArgument(T value) {
    using Unsigned = std::make_unsigned_t<T>;
    return FormatInteger{static_cast<std::uint64_t>(static_cast<Unsigned>(value)),
        static_cast<unsigned>(sizeof(T) * CHAR_BIT), std::is_signed_v<T> && value < 0, std::is_signed_v<T>};
}

template<class T> requires ((std::is_enum_v<T> && requires {
    FormatEnumTraits<T>::name;
    FormatEnumTraits<T>::members;
}) || HasNumericEnumFormatMetadata<T>)
TypedFormatArgument ngFormatArgument(T value) {
    using Traits = std::conditional_t<HasNumericEnumFormatMetadata<T>,
        FormatNumericEnumTraits<T>, FormatEnumTraits<T>>;
    const auto underlying = [&] {
        if constexpr (HasNumericEnumFormatMetadata<T>) return ngFormatArgument(Traits::underlying(value));
        else return ngFormatArgument(static_cast<std::underlying_type_t<T>>(value));
    }();
    FormatEnum result;
    const auto retainLabel = [](auto&& label) {
        using Label = std::remove_cvref_t<decltype(label)>;
        if constexpr (std::same_as<Label, std::string> || std::same_as<Label, std::string_view>)
            return std::get<compat::RetainedStringSlice>(ngFormatArgument(std::forward<decltype(label)>(label)));
        else return compat::RetainedStringSlice(compat::RetainedString(std::forward<decltype(label)>(label)));
    };
    std::visit([&](const auto& base) {
        using Base = std::remove_cvref_t<decltype(base)>;
        if constexpr (std::same_as<Base, FormatInteger> || std::same_as<Base, bool> ||
            std::same_as<Base, FormatCharacter>) result.base = base;
    }, underlying);
    result.typeName = retainLabel(Traits::name);
    for (const auto& member : Traits::members) {
        if (value == member.value) {
            result.memberName = retainLabel(member.name);
            break;
        }
    }
    return result;
}

template<class T>
TypedFormatArgument ngFormatArgument(compat::Slice<T> values) {
    auto range = [] {
        try { return std::make_shared<FormatRange>(); }
        catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    range->length = values.size();
    range->sourceTypeName=FormatSourceTypeName<compat::Slice<T>>::get();
    if constexpr (!FormatArrayElement<T>) {
        range->characterBits = static_cast<unsigned>(sizeof(T) * CHAR_BIT);
        range->characterView = std::basic_string_view<T>(values.data(), values.size());
    }
    // Retain the descriptor, not an eager copy of its element values. A later
    // element read observes writes through other views of the same allocation.
    try {
        range->read = [values = std::move(values)](std::size_t index) mutable {
            if constexpr (compat::SourceFixedArray<std::remove_cv_t<T>>::value)
                return ngFormatFixedArrayReferenceArgument(values[index], values.retentionOwner());
            else if constexpr (HasCustomFormat<T> || HasAggregateFormatMetadata<std::remove_cv_t<T>>)
                return ngFormatValueReferenceArgument(values[index], values.retentionOwner());
            else return ngFormatArgument(values[index]);
        };
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return range;
}

template<class Array> requires compat::SourceFixedArray<std::remove_cv_t<Array>>::value
TypedFormatArgument ngFormatFixedArrayReferenceArgument(Array& value, std::shared_ptr<void> owner) {
    using Traits = compat::SourceFixedArray<std::remove_cv_t<Array>>;
    using Element = typename Traits::Element;
    auto range = std::allocate_shared<FormatRange>(
        nicxlive::core::d_runtime::SourceAllocator<FormatRange>{});
    range->length = Traits::length;
    range->sourceTypeName = FormatSourceTypeName<compat::Slice<Element>>::get();
    if constexpr (!FormatArrayElement<Element>) {
        range->characterBits = static_cast<unsigned>(sizeof(Element) * CHAR_BIT);
        range->characterView = std::basic_string_view<Element>(value.data(), value.size());
    }
    try {
        range->read = [&value, owner = std::move(owner)](std::size_t index) {
            if constexpr (compat::SourceFixedArray<std::remove_cv_t<Element>>::value)
                return ngFormatFixedArrayReferenceArgument(value[index], owner);
            else if constexpr (HasCustomFormat<Element> || HasAggregateFormatMetadata<std::remove_cv_t<Element>>)
                return ngFormatValueReferenceArgument(value[index], owner);
            else return ngFormatArgument(value[index]);
        };
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return range;
}

namespace detail {
struct DynamicFormatField {
    bool dynamic = false;
    std::optional<std::size_t> argument;
};
struct ParsedTypedFormat : DecimalFormatSpec {
    bool center = false;
    bool grouping = false;
    int groupSize = 0;
    char32_t separator = ',';
    bool dynamicGroup = false;
    bool dynamicSeparator = false;
    std::optional<std::size_t> argument;
    std::optional<std::size_t> lastArgument;
    std::optional<std::size_t> consumeThrough;
    bool remainingArguments = false;
    std::string nested;
    std::optional<std::string> nestedSeparator;
    DynamicFormatField dynamicWidth;
    DynamicFormatField dynamicPrecision;
};

inline bool formatDigit(char value) { return value >= '0' && value <= '9'; }
inline char formatSpecAt(std::string_view text,std::size_t index,std::size_t line) {
    if (index>=text.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index,text.size(),
        compat::RetainedString::fromStaticLiteral("std/format/spec.d"),line);
    return text[index];
}
inline std::int64_t parseFormatDecimal(std::string_view& input,bool signedTarget,
        unsigned bits=32) {
    auto remaining=input;
    bool negative=false;
    if (signedTarget && !remaining.empty() && (remaining.front()=='-' || remaining.front()=='+')) {
        negative=remaining.front()=='-'; remaining.remove_prefix(1);
    }
    if (remaining.empty() || !formatDigit(remaining.front()))
        formatParseFailure(remaining,signedTarget?"int":"uint");
    const std::uint64_t maximum=signedTarget?std::uint64_t(INT_MAX)+(negative?1u:0u):UINT32_MAX;
    std::uint64_t value=0;
    while (!remaining.empty() && formatDigit(remaining.front())) {
        const auto digit=static_cast<unsigned>(remaining.front()-'0');
        if (value>maximum/10 || (value==maximum/10 && digit>maximum%10))
            formatConversionOverflow("Overflow in integral conversion",2527);
        value=value*10+digit; remaining.remove_prefix(1);
    }
    if (bits==16 && value>65535) formatConversionOverflow("Overflow in integral conversion",2449);
    input=remaining;
    return negative?-static_cast<std::int64_t>(value):static_cast<std::int64_t>(value);
}
inline ParsedTypedFormat parseTypedFormat(std::string_view pattern,std::size_t& offset,
        FormatSpec* publicSpec = nullptr) {
    ParsedTypedFormat result;
    const auto original = publicSpec ? publicSpec->trailing : compat::RetainedStringSlice{};
    if (publicSpec) {
        publicSpec->allFlags = 0; publicSpec->width = 0;
        publicSpec->indexStart = 0; publicSpec->indexEnd = 0;
        publicSpec->precision = FormatSpec::UNSPECIFIED; publicSpec->nested.reset();
    }
    auto trailing=pattern.substr(offset);
    const auto retainTrailing = [&] {
        if (publicSpec) publicSpec->trailing = original.slice(
            static_cast<std::size_t>(trailing.data() - pattern.data()), original.size());
    };
    for (std::size_t i=0;i<trailing.size();) {
        switch (trailing[i]) {
        case '-': result.left=true; if(publicSpec) publicSpec->flDash=true; ++i; break;
        case '+': result.plus=true; if(publicSpec) publicSpec->flPlus=true; ++i; break;
        case ' ': result.space=true; if(publicSpec) publicSpec->flSpace=true; ++i; break;
        case '0': result.zero=true; if(publicSpec) publicSpec->flZero=true; ++i; break;
        case '#': result.alternate=true; if(publicSpec) publicSpec->flHash=true; ++i; break;
        case '=': result.center=true; if(publicSpec) publicSpec->flEqual=true; ++i; break;
        case '*': {
            if (formatDigit(formatSpecAt(trailing,++i,378))) {
                trailing.remove_prefix(1);
                retainTrailing();
                const auto position=parseFormatDecimal(trailing,true);
                retainTrailing();
                if (publicSpec) publicSpec->width=std::bit_cast<std::int32_t>(
                    0u-static_cast<std::uint32_t>(position));
                i=0;
                if (formatSpecAt(trailing,i++,385)!='$')
                    formatFailure(allocateAggregateText([&] {
                        return "$ expected after '*"+std::to_string(position)+"' in format string";
                    }),
                        "std/format/spec.d",385);
                result.dynamicWidth=position?DynamicFormatField{true,static_cast<std::size_t>(position-1)}
                    :DynamicFormatField{};
                result.width=0;
            } else {
                result.dynamicWidth=DynamicFormatField{true,{}};
                if(publicSpec) publicSpec->width=FormatSpec::DYNAMIC;
            }
            break;
        }
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
            auto tmp=trailing.substr(i);
            const auto value=parseFormatDecimal(tmp,false);
            if (tmp.empty()) formatFailure(allocateAggregateText([&] {
                return "Incorrect format specifier %"+std::string(trailing.substr(i));
            }),
                "std/format/spec.d",397);
            i=trailing.size()-tmp.size();
            if (tmp.front()=='$' || tmp.front()==':') {
                if (value>255) formatConversionOverflow("Conversion positive overflow",1587);
                result.argument=static_cast<std::size_t>(value-1);
                if(publicSpec) publicSpec->indexEnd=publicSpec->indexStart=static_cast<std::uint16_t>(value);
                result.lastArgument=result.argument;
                result.consumeThrough=static_cast<std::size_t>(value);
                if (tmp.front()=='$') ++i;
                else {
                    if(publicSpec) publicSpec->indexEnd=0;
                    tmp.remove_prefix(1);
                    if (!tmp.empty() && tmp.front()=='$') {
                        result.remainingArguments=true;
                        if(publicSpec) publicSpec->indexEnd=UINT16_MAX;
                    }
                    else {
                        const auto last=parseFormatDecimal(tmp,false,16);
                        if(publicSpec) publicSpec->indexEnd=static_cast<std::uint16_t>(last);
                        result.remainingArguments=last==65535;
                        result.lastArgument=last?static_cast<std::size_t>(last-1):0;
                        result.consumeThrough=static_cast<std::size_t>(last);
                    }
                    i=trailing.size()-tmp.size();
                    if (formatSpecAt(trailing,i++,420)!='$')
                        formatFailure("$ expected","std/format/spec.d",420);
                }
            } else {
                if (value>INT_MAX) formatConversionOverflow("Conversion positive overflow",562);
                result.width=static_cast<std::size_t>(value);
                if(publicSpec) publicSpec->width=static_cast<int>(value);
                result.dynamicWidth=value==INT_MAX?DynamicFormatField{true,{}}:DynamicFormatField{};
            }
            break;
        }
        case ',': {
            result.grouping=true; ++i;
            if(publicSpec) publicSpec->flSeparator=true;
            const auto next=formatSpecAt(trailing,i,434);
            if (next=='*') {
                result.dynamicGroup=true; ++i;
                if(publicSpec) publicSpec->separators=FormatSpec::DYNAMIC;
            }
            else if (formatDigit(next)) {
                auto tmp=trailing.substr(i);
                result.groupSize=static_cast<int>(parseFormatDecimal(tmp,true));
                if(publicSpec) publicSpec->separators=result.groupSize;
                i=trailing.size()-tmp.size(); result.dynamicGroup=result.groupSize==INT_MAX;
            } else {
                result.groupSize=3; result.dynamicGroup=false;
                if(publicSpec) publicSpec->separators=3;
            }
            if (formatSpecAt(trailing,i,452)=='?') {
                result.dynamicSeparator=true; ++i;
                if(publicSpec) publicSpec->dynamicSeparatorChar=true;
            }
            break;
        }
        case '.': {
            if (formatSpecAt(trailing,++i,461)=='*') {
                if (formatDigit(formatSpecAt(trailing,++i,463))) {
                    trailing.remove_prefix(i); i=0;
                    retainTrailing();
                    const auto position=parseFormatDecimal(trailing,true);
                    retainTrailing();
                    if(publicSpec) publicSpec->precision=std::bit_cast<std::int32_t>(
                        0u-static_cast<std::uint32_t>(position));
                    if (formatSpecAt(trailing,i++,470)!='$')
                        formatFailure("$ expected","std/format/spec.d",470);
                    result.dynamicPrecision=position?DynamicFormatField{true,static_cast<std::size_t>(position-1)}
                        :DynamicFormatField{};
                    result.precision=position?std::optional<std::size_t>{}:std::optional<std::size_t>{0};
                } else {
                    result.dynamicPrecision=DynamicFormatField{true,{}}; result.precision.reset();
                    if(publicSpec) publicSpec->precision=FormatSpec::DYNAMIC;
                }
            } else if (trailing[i]=='-') {
                if(publicSpec) publicSpec->precision=0;
                auto tmp=trailing.substr(i); (void)parseFormatDecimal(tmp,true);
                i=trailing.size()-tmp.size(); result.precision=0; result.dynamicPrecision={};
            } else if (formatDigit(trailing[i])) {
                auto tmp=trailing.substr(i);
                const auto precision=parseFormatDecimal(tmp,true);
                if(publicSpec) publicSpec->precision=static_cast<int>(precision);
                i=trailing.size()-tmp.size(); result.precision=static_cast<std::size_t>(precision);
                result.dynamicPrecision=precision==INT_MAX?DynamicFormatField{true,{}}:DynamicFormatField{};
            } else {
                result.precision=0; result.dynamicPrecision={};
                if(publicSpec) publicSpec->precision=0;
            }
            break;
        }
        case '(': {
            auto j=i+1; std::size_t nesting=0;
            for (;;) {
                if (j+1>=trailing.size()) formatFailure(allocateAggregateText([&] {
                    return "Incorrect format specifier: %"+std::string(trailing.substr(i));
                }),"std/format/spec.d",316);
                if (trailing[j++]!='%') continue;
                if (trailing[j]=='-') {
                    if (++j>=trailing.size()) formatFailure(allocateAggregateText([&] {
                        return "Incorrect format specifier: %"+std::string(trailing.substr(i));
                    }),"std/format/spec.d",326);
                }
                if (trailing[j]==')') { if (nesting==0) break; --nesting; }
                else if (trailing[j]=='|') { if (nesting==0) break; }
                else if (trailing[j]=='(') ++nesting;
            }
            if (trailing[j]=='|') {
                const auto k=j;
                for (++j;;) {
                    if (formatSpecAt(trailing,j++,347)!='%') continue;
                    const auto token=formatSpecAt(trailing,j,349);
                    if (token=='%') ++j;
                    else if (token==')') break;
                    else formatFailure(allocateAggregateText([&] {
                        return "Incorrect format specifier: %"+std::string(trailing.substr(j));
                    }),
                        "std/format/spec.d",354);
                }
                if (!publicSpec) {
                    allocateAggregateText([&] { result.nested=trailing.substr(i+1,k-i-2); });
                    allocateAggregateText([&] {
                        result.nestedSeparator=std::string(trailing.substr(k+1,j-k-2));
                    });
                }
                if(publicSpec) {
                    const auto base=static_cast<std::size_t>(trailing.data()-pattern.data());
                    publicSpec->nested=original.slice(base+i+1,base+k-1);
                    publicSpec->sep=original.slice(base+k+1,base+j-1);
                }
            } else {
                if (!publicSpec) allocateAggregateText([&] { result.nested=trailing.substr(i+1,j-i-2); });
                result.nestedSeparator.reset();
                if(publicSpec) {
                    const auto base=static_cast<std::size_t>(trailing.data()-pattern.data());
                    publicSpec->nested=original.slice(base+i+1,base+j-1); publicSpec->sep.reset();
                }
            }
            result.conversion='('; offset=static_cast<std::size_t>(trailing.data()-pattern.data())+j+1;
            if(publicSpec) { publicSpec->spec='('; publicSpec->trailing=original.slice(offset,original.size()); }
            return result;
        }
        default:
            result.conversion=trailing[i++];
            if(publicSpec) publicSpec->spec=result.conversion;
            offset=static_cast<std::size_t>(trailing.data()-pattern.data())+i;
            if(publicSpec) publicSpec->trailing=original.slice(offset,original.size());
            if (result.precision && *result.precision==static_cast<std::size_t>(INT_MAX-1))
                result.precision.reset();
            return result;
        }
    }
    formatFailure(allocateAggregateText([&] {
        return "Incorrect format specifier: "+std::string(trailing);
    }),"std/format/spec.d",506);
}

inline std::string formatSourceTypeName(const TypedFormatArgument& argument) {
    return std::visit([](const auto& value)->std::string {
        using T=std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<T,compat::RetainedStringSlice>) return copyFormatTypeName("string");
        else if constexpr (std::same_as<T,FormatInteger>) {
            if (value.bits==8) return copyFormatTypeName(value.signedType?"byte":"ubyte");
            if (value.bits==16) return copyFormatTypeName(value.signedType?"short":"ushort");
            if (value.bits==32) return copyFormatTypeName(value.signedType?"int":"uint");
            return copyFormatTypeName(value.signedType?"long":"ulong");
        } else if constexpr (std::same_as<T,FormatEnum>) return copyFormatTypeName(value.typeName.view());
        else if constexpr (std::same_as<T,FormatObject>) return copyFormatTypeName(value.sourceTypeName.view());
        else if constexpr (std::same_as<T,FormatPointer>) return copyFormatTypeName(value.sourceTypeName);
        else if constexpr (std::same_as<T,FormatCharacter>)
            return copyFormatTypeName(value.bits==8?"char":value.bits==16?"wchar":"dchar");
        else if constexpr (std::same_as<T,bool>) return copyFormatTypeName("bool");
        else if constexpr (std::same_as<T,float>) return copyFormatTypeName("float");
        else if constexpr (std::same_as<T,double>) return copyFormatTypeName("double");
        else if constexpr (FormatExtendedReal<T>) return copyFormatTypeName("real");
        else {
            if (!value || value->sourceTypeName.empty())
                throw std::logic_error("Source format range type metadata is unavailable");
            return copyFormatTypeName(value->sourceTypeName);
        }
    },argument);
}
inline int dynamicFormatInteger(const TypedFormatArgument& argument,std::string_view kind,std::size_t index) {
    auto value = std::get_if<FormatInteger>(&argument);
    if (const auto enumeration = std::get_if<FormatEnum>(&argument))
        value = std::get_if<FormatInteger>(&enumeration->base);
    if (!value) {
        const auto typeName = formatSourceTypeName(argument);
        formatFailure(allocateAggregateText([&] {
            return std::string(kind)+" expected, not "+typeName+
                " for argument #"+std::to_string(index+1);
        }),"std/format/internal/write.d",3260);
    }
    const auto mask = value->bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << value->bits) - 1;
    const auto magnitude = value->negative ? (~value->raw + 1) & mask : value->raw;
    if (magnitude > static_cast<std::uint64_t>(INT_MAX) + (value->negative ? 1 : 0)) {
        if (value->negative) formatConversionOverflow("Conversion negative overflow",1581);
        formatConversionOverflow("Conversion positive overflow",!value->signedType && value->bits==32?562:1587);
    }
    if (value->negative && magnitude == static_cast<std::uint64_t>(INT_MAX) + 1) return INT_MIN;
    return value->negative ? -static_cast<int>(magnitude) : static_cast<int>(magnitude);
}
inline std::string alignTypedFormat(std::string prefix, std::string body, const ParsedTypedFormat& spec,
    bool numeric, bool integerPrecision = false, std::size_t textWidth = 0) {
    const auto visible = numeric ? prefix.size() + body.size() : textWidth;
    const auto padding = spec.width > visible ? spec.width - visible : 0;
    if (spec.center && !(numeric && spec.zero && !integerPrecision)) {
        const auto left = padding / 2 + (padding % 2 && !spec.left ? 1 : 0);
        return std::string(left, ' ') + prefix + body + std::string(padding - left, ' ');
    }
    if (spec.left) return prefix + body + std::string(padding, ' ');
    if (numeric && spec.zero && !integerPrecision) return prefix + std::string(padding, '0') + body;
    return std::string(padding, ' ') + prefix + body;
}
inline std::string formatTypedText(std::string_view text, const ParsedTypedFormat& spec) {
    if (spec.conversion == 'r') return allocateAggregateText([&] {
        return text.empty() ? std::string{} : std::string(text);
    });
    if (spec.conversion != 's') formatFailure(allocateAggregateText([&] {
        return std::string("Incorrect format specifier for range: %")+spec.conversion;
    }),"std/format/internal/write.d",1588);
    if (spec.precision && *spec.precision < text.size()) text = text.substr(0, *spec.precision);
    // Source byte-string precision slices bytes; width counts source graphemes only if needed.
    const auto width = spec.width ? formatDisplayWidth(text) : 0;
    auto payload = allocateAggregateText([&] { return std::string(text); });
    return alignTypedFormat({}, std::move(payload), spec, false, false, width);
}
struct TypedIntegerComponents {
    std::string digits, prefix;
    std::uint64_t magnitude;
    unsigned base;
    bool general, hexFloat, fixed, floatingStyle, upper;
};
inline TypedIntegerComponents typedIntegerComponents(FormatInteger value, char conversion, bool plus, bool space) {
    unsigned base = 10;
    const bool unsignedForm = conversion == 'u' || conversion == 'x' || conversion == 'X' ||
        conversion == 'o' || conversion == 'b';
    const bool general = conversion == 'g' || conversion == 'G';
    const bool hexFloat = conversion == 'a' || conversion == 'A';
    const bool fixed = conversion == 'f' || conversion == 'F';
    const bool scientific = conversion == 'e' || conversion == 'E';
    const bool floatingStyle = general || hexFloat || fixed || scientific;
    if (conversion == 'x' || conversion == 'X' || hexFloat) base = 16;
    else if (conversion == 'o') base = 8;
    else if (conversion == 'b') base = 2;
    else if (conversion != 's' && conversion != 'd' && conversion != 'u' && !floatingStyle)
        formatFailure(std::string("incompatible format character for integral argument: %")+
            conversion,"std/format/internal/write.d",340);
    const auto mask = value.bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << value.bits) - 1;
    const auto magnitude = !unsignedForm && value.negative ? (~value.raw + 1) & mask : value.raw;
    char bytes[65];
    const auto converted = std::to_chars(bytes, bytes + sizeof(bytes), magnitude, base);
    std::string digits(bytes, converted.ptr);
    const bool upper = conversion == 'X' || conversion == 'A';
    if (upper) for (auto& c : digits) if (c >= 'a' && c <= 'f') c -= 'a' - 'A';
    std::string prefix;
    if (!unsignedForm) prefix = value.negative ? "-" : plus ? "+" : space ? " " : "";
    return {std::move(digits), std::move(prefix), magnitude, base, general, hexFloat, fixed, floatingStyle, upper};
}
inline std::string formatTypedInteger(FormatInteger value, const ParsedTypedFormat& spec) {
    const auto conversion = spec.conversion;
    if (conversion == 'r') return formatRawBits(value.raw, value.bits, spec.plus, spec.left);
    auto parts = typedIntegerComponents(value, conversion, spec.plus, spec.space);
    auto& digits = parts.digits;
    auto& prefix = parts.prefix;
    const auto magnitude = parts.magnitude;
    const auto base = parts.base;
    const auto general = parts.general, hexFloat = parts.hexFloat, fixed = parts.fixed;
    const auto floatingStyle = parts.floatingStyle, upper = parts.upper;
    if (floatingStyle) {
        // Integral arguments retain exact base digits. In particular, source %g
        // chooses fixed notation at precision >= digit count - 1, not the
        // floating-point %g threshold; no integer-to-double conversion occurs.
        auto precision = spec.precision.value_or(digits.size() - 1);
        std::string fraction;
        if (fixed || (general && precision >= digits.size() - 1)) {
            if (!spec.precision) precision = 0;
            auto zeros = fixed ? precision : spec.alternate && precision > digits.size() ?
                precision - digits.size() : 0;
            if (zeros || spec.alternate) fraction = "." + std::string(zeros, '0');
            return formatNumericLayout(prefix, digits, fraction, {}, spec);
        }
        if (general && !precision) precision = 1;
        const auto keep = precision + (general ? 0 : 1);
        auto exponent = digits.size() - 1;
        if (keep < digits.size()) {
            const auto first = formatHexDigit(digits[keep]);
            const auto tie = base / 2;
            const bool tail = std::any_of(digits.begin() + keep + 1, digits.end(),
                [](char digit) { return digit != '0'; });
            const auto retained = formatHexDigit(digits[keep - 1]);
            const char roundingClass = first < tie ? (first ? '1' : '0') : first == tie ? '5' : '9';
            digits.resize(keep);
            if (incrementRoundedFormat(value.negative, first != 0 || tail, roundingClass,
                tail, (retained & 1) != 0)) {
                bool carry = true;
                for (auto i = digits.size(); i && carry; --i) {
                    const auto digit = formatHexDigit(digits[i - 1]);
                    carry = digit + 1 == base;
                    digits[i - 1] = carry ? '0' : formatHexCharacter(digit + 1, upper);
                }
                if (carry) { digits.insert(digits.begin(), '1'); digits.resize(keep); ++exponent; }
            }
        }
        if (hexFloat) { prefix += upper ? "0X" : "0x"; exponent *= 4; }
        auto fractional = digits.substr(1);
        if (general && !spec.alternate) {
            while (!fractional.empty() && fractional.back() == '0') fractional.pop_back();
        } else {
            const auto requested = general ? precision - 1 : precision;
            if (fractional.size() < requested) fractional.append(requested - fractional.size(), '0');
        }
        if (!fractional.empty() || spec.alternate) fraction = "." + fractional;
        std::string suffix(1, hexFloat ? (upper ? 'P' : 'p') :
            (conversion == 'E' || conversion == 'G' ? 'E' : 'e'));
        suffix += '+';
        // The source integer overload always writes two exponent digits,
        // including hexadecimal forms (unlike the actual float overload).
        suffix.push_back(static_cast<char>('0' + exponent / 10));
        suffix.push_back(static_cast<char>('0' + exponent % 10));
        return formatNumericLayout(prefix, std::string_view(digits).substr(0, 1), fraction, suffix, spec);
    }
    if (spec.alternate && magnitude && base == 16) prefix = conversion == 'X' ? "0X" : "0x";
    if (spec.alternate && magnitude && base == 8 && (!spec.precision || *spec.precision <= digits.size()))
        prefix = "0";
    if (spec.precision && digits.size() < *spec.precision) digits.insert(0, *spec.precision - digits.size(), '0');
    return formatNumericLayout(prefix, digits, {}, {}, spec, spec.precision.has_value());
}
template<class T> std::string formatTypedFloating(T value, const ParsedTypedFormat& spec) {
    if (spec.conversion == 'r') {
        if constexpr (FormatExtendedReal<T>) {
            // Source raw formatting copies the actual target value representation,
            // including padding; no payload normalization or numeric conversion.
            std::string result(reinterpret_cast<const char*>(&value), sizeof(value));
            const bool swap = (std::endian::native == std::endian::little && spec.plus)
                || (std::endian::native == std::endian::big && spec.left);
            if (swap) std::reverse(result.begin(), result.end());
            return result;
        } else {
            using Bits = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;
            return formatRawBits(static_cast<std::uint64_t>(std::bit_cast<Bits>(value)),
                static_cast<unsigned>(sizeof(T) * CHAR_BIT), spec.plus, spec.left);
        }
    }
    if (spec.conversion!='s' && spec.conversion!='e' && spec.conversion!='E' &&
        spec.conversion!='f' && spec.conversion!='F' && spec.conversion!='g' &&
        spec.conversion!='G' && spec.conversion!='a' && spec.conversion!='A')
        formatFailure(std::string("incompatible format character for floating point argument: %")+
            spec.conversion,"std/format/internal/write.d",602);
    if (spec.conversion == 'a' || spec.conversion == 'A') {
        const auto parts = makeHexFormatParts(value, spec);
        return formatNumericLayout(parts.prefix, parts.integral, parts.fraction, parts.suffix, spec, false, parts.special);
    }
    DecimalFormatSpec numeric = spec;
    numeric.width = 0;
    numeric.left = false;
    numeric.zero = false;
    auto rendered = ngFormatDecimal(value, numeric);
    std::string_view body = rendered;
    std::string_view prefix;
    if (!body.empty() && (body[0] == '-' || body[0] == '+' || body[0] == ' ')) {
        prefix = body.substr(0, 1);
        body.remove_prefix(1);
    }
    if (!std::isfinite(value)) return formatNumericLayout(prefix, {}, {}, body, spec, false, true);
    const auto exponent = body.find_first_of("eE");
    const auto mantissa = body.substr(0, exponent);
    const auto dot = mantissa.find('.');
    const auto integral = mantissa.substr(0, dot);
    const auto fraction = dot == std::string_view::npos ? std::string_view{} : mantissa.substr(dot);
    const auto suffix = exponent == std::string_view::npos ? std::string_view{} : body.substr(exponent);
    return formatNumericLayout(prefix, integral, fraction, suffix, spec);
}
inline std::string formatTypedArgument(const TypedFormatArgument& argument, const ParsedTypedFormat& spec);
inline std::vector<char32_t> formatRangeCodeUnits(const FormatRange& range, std::size_t count) {
    std::vector<char32_t> units;
    allocateAggregateText([&] { units.reserve(count); });
    for (std::size_t i = 0; i < count; ++i) {
        const auto unit = std::get<FormatCharacter>(range.read(i)).value;
        allocateAggregateText([&] { units.push_back(unit); });
    }
    return units;
}
struct FormatCodeUnitView {
    const FormatRange* range;
    std::size_t length;
    std::size_t size() const { return length; }
    char32_t operator[](std::size_t index) const {
        return std::get<FormatCharacter>(range->read(index)).value;
    }
};
template<class Unit> FormatRange decodedFormatRange(const FormatRange& source) {
    using Cursor=compat::phobos::detail::UnicodeCursor<FormatCodeUnitView,Unit>;
    const auto cursor=std::allocate_shared<Cursor>(
        nicxlive::core::d_runtime::SourceAllocator<Cursor>{}, FormatCodeUnitView{&source,source.length});
    FormatRange result;
    result.length = source.length;
    try {
        result.read=[cursor](std::size_t)->TypedFormatArgument {
            return FormatCharacter{cursor->front(),32};
        };
        result.empty=[cursor] { return cursor->empty(); };
        result.popFront=[cursor] { cursor->popFront(); };
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return result;
}
inline std::string formatTypedElement(const TypedFormatArgument& argument, const ParsedTypedFormat& spec) {
    if (spec.conversion == 's') {
        if (const auto text = std::get_if<compat::RetainedStringSlice>(&argument))
            return formatQuotedString(text->view());
        if (const auto character = std::get_if<FormatCharacter>(&argument)) {
            auto text = formatQuotedCodepoint(character->value, '\'');
            return allocateAggregateText([&] { return "'" + std::move(text) + "'"; });
        }
        if (const auto range = std::get_if<std::shared_ptr<FormatRange>>(&argument);
            range && *range && (*range)->characterBits)
            return formatQuotedCodeUnits(formatRangeCodeUnits(**range, (*range)->length), (*range)->characterBits);
    }
    return formatTypedArgument(argument, spec);
}
struct NestedFormatOutput { std::string body; std::string_view trailing; };
template<class Read> inline NestedFormatOutput formatNestedElement(std::string_view pattern,
    const Read& read, bool unquoted) {
    NestedFormatOutput result;
    std::size_t offset = 0;
    while (offset < pattern.size()) {
        if (pattern[offset] != '%') {
            allocateAggregateText([&] { result.body.push_back(pattern[offset++]); });
            continue;
        }
        ++offset;
        if (offset==pattern.size())
            formatFailure("Unterminated format specifier: \"%\"","std/format/spec.d",262);
        if (offset < pattern.size() && pattern[offset] == '%') {
            allocateAggregateText([&] { result.body.push_back('%'); });
            ++offset; continue;
        }
        const auto elementSpec = parseTypedFormat(pattern, offset);
        const auto value = read();
        const auto text = unquoted ? formatTypedArgument(value, elementSpec) : formatTypedElement(value, elementSpec);
        allocateAggregateText([&] { result.body += text; });
        // The source deliberately leaves a literal suffix pending after the
        // final conversion. Without %|, it is emitted only between elements.
        if (pattern.substr(offset).find('%') == std::string_view::npos) {
            result.trailing = pattern.substr(offset);
            return result;
        }
    }
    return result;
}
inline std::string formatTypedRange(const FormatRange& range, const ParsedTypedFormat& spec) {
    if (spec.conversion != 's' && spec.conversion != 'r' && spec.conversion != '(')
        formatFailure(allocateAggregateText([&] {
            return std::string("Incorrect format specifier for range: %")+spec.conversion;
        }), "std/format/internal/write.d",1588);
    if (range.characterBits) {
        if (spec.conversion == 's') {
            // String precision slices original code units before transcoding,
            // not UTF-8 bytes produced from a wider input or decoded scalars.
            const auto count = std::min(range.length, spec.precision.value_or(range.length));
            const auto text = encodeFormatCodeUnits(formatRangeCodeUnits(range, count), range.characterBits);
            auto stringSpec = allocateAggregateText([&] { return spec; });
            stringSpec.precision.reset();
            return formatTypedText(text, stringSpec);
        }
        if (spec.conversion == '(') {
            if (range.characterBits==8) return formatTypedRange(decodedFormatRange<char>(range),spec);
            if (range.characterBits==16) return formatTypedRange(decodedFormatRange<char16_t>(range),spec);
            // dchar front is an actual indexed value; source range traversal
            // does not introduce UTF-32 validation before element formatting.
            const auto position=std::allocate_shared<std::size_t>(
                nicxlive::core::d_runtime::SourceAllocator<std::size_t>{}, 0);
            FormatRange decoded;
            decoded.length = range.length;
            try {
                decoded.read=[&range,position](std::size_t)->TypedFormatArgument {
                    return range.read(*position);
                };
                decoded.empty=[position,count=range.length] { return *position==count; };
                decoded.popFront=[position] { ++*position; };
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
            return formatTypedRange(decoded, spec);
        }
        // Raw output below walks the original code units and their bit widths;
        // it must never transcode or combine UTF-16 surrogate pairs.
    }
    std::string result = allocateAggregateText([&] { return std::string(spec.conversion == 's' ? "[" : ""); });
    const auto count = range.length;
    for (std::size_t index = 0; index < count && (!range.empty || !range.empty()); ++index) {
        if (spec.conversion == '(') {
            // Re-read front for each conversion, not just once per element.
            // A custom formatter may change this actual array slot between
            // conversions while the retained range descriptor stays fixed.
            const auto element = formatNestedElement(spec.nested, [&range, index] {
                return range.read(index);
            }, spec.left);
            allocateAggregateText([&] { result += element.body; });
            if (range.popFront) range.popFront();
            const bool more=range.empty?!range.empty():index+1<count;
            if (spec.nestedSeparator) {
                allocateAggregateText([&] { result += element.trailing; });
                if (more) allocateAggregateText([&] { result += *spec.nestedSeparator; });
            } else if (more) allocateAggregateText([&] { result += element.trailing; });
        } else {
            const auto value = range.read(index);
            if (spec.conversion == 's' && index) allocateAggregateText([&] { result += ", "; });
            const auto text = spec.conversion == 's' ? formatTypedElement(value, spec) : formatTypedArgument(value, spec);
            allocateAggregateText([&] { result += text; });
            if (range.popFront) range.popFront();
        }
    }
    if (spec.conversion == 's') allocateAggregateText([&] { result += ']'; });
    return result;
}
inline std::string formatTypedEnum(const FormatEnum& value, const ParsedTypedFormat& spec) {
    const auto base = [&](const ParsedTypedFormat& baseSpec) {
        return std::visit([&](const auto& item) {
            return formatTypedArgument(TypedFormatArgument{item}, baseSpec);
        }, value.base);
    };
    if (spec.conversion != 's') return base(spec);
    if (value.memberName) return formatTypedText(value.memberName->view(), spec);
    auto baseSpec = allocateAggregateText([&] { return spec; });
    baseSpec.width = 0;
    auto text = allocateAggregateText([&] {
        return std::string("cast(") + std::string(value.typeName.view()) + ")";
    });
    const auto raw = base(baseSpec);
    allocateAggregateText([&] { text += raw; });
    // The source aligns the entire fallback but applies precision only while
    // formatting the base value; it does not slice the cast expression again.
    const auto width = spec.width ? formatDisplayWidth(text) : 0;
    return alignTypedFormat({}, std::move(text), spec, false, false, width);
}
inline std::string formatTypedArgument(const TypedFormatArgument& argument, const ParsedTypedFormat& spec) {
    // formatElement's direct quoted string/character cases intentionally bypass
    // formatValue; nested dynamic flags are rejected only on this real path.
    if (spec.dynamicWidth.dynamic || spec.dynamicPrecision.dynamic || spec.dynamicGroup || spec.dynamicSeparator ||
        spec.width==static_cast<std::size_t>(INT_MAX) ||
        (spec.precision && *spec.precision==static_cast<std::size_t>(INT_MAX)) || spec.groupSize==INT_MAX)
        formatFailure("Dynamic argument not allowed for `formatValue`","std/format/write.d",1259);
    return std::visit([&](const auto& value) -> std::string {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<T, compat::RetainedStringSlice>) {
            if (spec.conversion == '(') {
                using Cursor=compat::phobos::detail::UnicodeCursor<compat::RetainedStringSlice,char>;
                const auto cursor=std::allocate_shared<Cursor>(
                    nicxlive::core::d_runtime::SourceAllocator<Cursor>{}, value);
                FormatRange characters;
                characters.length = value.size();
                try {
                    characters.read = [cursor](std::size_t) -> TypedFormatArgument {
                        return FormatCharacter{cursor->front(),32};
                    };
                    characters.empty=[cursor] { return cursor->empty(); };
                    characters.popFront=[cursor] { cursor->popFront(); };
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
                return formatTypedRange(characters, spec);
            }
            return formatTypedText(value.view(), spec);
        }
        else if constexpr (std::same_as<T, std::shared_ptr<FormatRange>>) {
            if (!value) throw std::invalid_argument("Missing retained format range");
            return formatTypedRange(*value, spec);
        }
        else if constexpr (std::same_as<T, FormatEnum>) return formatTypedEnum(value, spec);
        else if constexpr (std::same_as<T, FormatObject>) {
            // std.format.internal.write.enforceValidFormatSpec/formatObject:
            // a no-argument toString accepts only %s, then writes its result
            // directly; the outer width and precision do not format it again.
            if (!value.acceptsFormatSpec && spec.conversion != 's') {
                if (value.sourceTypeName.empty())
                    throw std::logic_error("Source custom format type metadata is unavailable");
                formatFailure(allocateAggregateText([&] {
                    return "Expected '%s' format specifier for type '"+
                        std::string(value.sourceTypeName.view())+"'";
                }), "std/format/internal/write.d", 2862);
            }
            if (value.format) {
                const auto text = value.format(ngPublicFormatSpec(spec));
                return allocateAggregateText([&] { return std::string(text.view()); });
            }
            const auto text = value.toString();
            return allocateAggregateText([&] { return std::string(text.view()); });
        }
        else if constexpr (std::same_as<T, std::shared_ptr<FormatAggregate>>) {
            if (!value || value->sourceTypeName.empty())
                throw std::logic_error("Source aggregate field metadata is unavailable");
            if (spec.conversion != 's') formatFailure(allocateAggregateText([&] {
                return "Expected '%s' format specifier for type '"+value->sourceTypeName+"'";
            }), "std/format/internal/write.d", 2862);
            if (value->isUnion)
                return allocateAggregateText([&] { return value->sourceTypeName; });
            std::string text=allocateAggregateText([&] { return value->sourceTypeName+"("; });
            for (std::size_t i=0;i<value->fields.size();++i) {
                const auto& field=value->fields[i];
                if (field.hiddenContext) continue;
                if (i && value->fields[i-1].offset==field.offset) {
                    allocateAggregateText([&] {
                        text+=", "; text+=field.name;
                        if (i+1==value->fields.size() || value->fields[i+1].offset!=field.offset) text+='}';
                    });
                } else if (i+1<value->fields.size() && value->fields[i+1].offset==field.offset) {
                    allocateAggregateText([&] {
                        if (i) text+=", ";
                        text+="#{overlap "; text+=field.name;
                    });
                } else {
                    if (i) allocateAggregateText([&] { text+=", "; });
                    const auto element = formatTypedElement(value->read(i),spec);
                    allocateAggregateText([&] { text+=element; });
                }
            }
            return allocateAggregateText([&] { return text+")"; });
        }
        else if constexpr (std::same_as<T, FormatInteger>) return formatTypedInteger(value, spec);
        else if constexpr (std::same_as<T, FormatPointer>) {
            // Phobos formats void* through ulong, with a word only for null %s.
            if (spec.conversion == 's' && value.address == 0)
                return alignTypedFormat({}, "null", spec, false, false, 4);
            auto pointerSpec = spec;
            if (pointerSpec.conversion == 's') pointerSpec.conversion = 'X';
            else if (pointerSpec.conversion != 'x' && pointerSpec.conversion != 'X')
                formatFailure("Expected one of %s, %x or %X for pointer type.",
                    "std/format/internal/write.d", 2977);
            return formatTypedInteger(FormatInteger{static_cast<std::uint64_t>(value.address), 64, false, false},
                pointerSpec);
        }
        else if constexpr (std::same_as<T, bool>) {
            if (spec.conversion == 's') {
                // Source bool formatting aligns a word; string precision slicing
                // belongs to the string range formatter, not this overload.
                auto wordSpec = allocateAggregateText([&] { return spec; });
                wordSpec.precision.reset();
                return formatTypedText(value ? "true" : "false", wordSpec);
            }
            return formatTypedInteger(FormatInteger{value ? 1u : 0u, 8, false}, spec);
        } else if constexpr (std::same_as<T, FormatCharacter>) {
            if (spec.conversion == 's' || spec.conversion == 'c') {
                auto characterSpec = allocateAggregateText([&] { return spec; });
                characterSpec.conversion = 's';
                characterSpec.precision.reset();
                // Same-encoding char output preserves its one raw byte. Wider
                // source character types must be transcoded to the UTF-8 writer.
                const auto text = value.bits == 8 ? allocateAggregateText([&] {
                    return std::string(1, static_cast<char>(value.value));
                }) :
                    formatSeparatorUtf8(value.value);
                return formatTypedText(text, characterSpec);
            }
            return formatTypedInteger(FormatInteger{static_cast<std::uint64_t>(value.value), value.bits, false}, spec);
        } else return formatTypedFloating(value, spec);
    }, argument);
}

struct IntegerVoidFormatBuffer {
    std::array<unsigned char, 64> bytes;
    unsigned char& at(std::size_t index, unsigned line) {
        if (index >= bytes.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, bytes.size(),
            compat::RetainedString::fromStaticLiteral("std/format/internal/write.d"), line);
        return bytes[index];
    }
    compat::RetainedStringSlice slice(std::size_t lower, std::size_t upper, unsigned line) {
        if (lower > upper || upper > bytes.size())
            throw compat::ngStaticError<compat::ArraySliceError>(lower, upper, bytes.size(),
                compat::RetainedString::fromStaticLiteral("std/format/internal/write.d"), line);
        return compat::RetainedString::fromBorrowedView(
            std::string_view(reinterpret_cast<const char*>(bytes.data() + lower), upper - lower));
    }
};

inline bool roundIntegerFormatBuffer(IntegerVoidFormatBuffer& buffer, std::size_t left,
    std::size_t right, char rounding, bool negative, unsigned char maximum) {
#ifndef NDEBUG
    const auto require = [](bool condition, unsigned line) {
        if (!condition) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("std/format/internal/write.d"), line);
    };
    require(left < buffer.bytes.size(), 3669);
    require(right <= buffer.bytes.size(), 3671);
    require(right >= left, 3672);
#endif
    bool up;
    switch (std::fegetround()) {
    case FE_UPWARD: up = rounding != '0' && !negative; break;
    case FE_DOWNWARD: up = rounding != '0' && negative; break;
    case FE_TOWARDZERO: up = false; break;
    case FE_TONEAREST:
        up = rounding == '9';
        if (rounding == '5') {
            auto last = buffer.at(right - 1, 3723);
            if (last == '.') last = buffer.at(right - 2, 3724);
            up = (last <= '9' && last % 2 != 0) || (last > '9' && last % 2 == 0);
        }
        break;
    default: compat::ngAssertUnreachable("Unknown floating point rounding mode",
        "std/format/internal/write.d", 3698);
    }
    if (!up) return false;
    for (auto index = right; index > left;) {
        --index;
        if (buffer.at(index, 3734) == '.') continue;
        if (buffer.at(index, 3735) == maximum) buffer.at(index, 3736) = '0';
        else {
            if (maximum != '9' && buffer.at(index, 3739) == '9')
                buffer.at(index, 3740) = maximum == 'f' ? 'a' : 'A';
            else ++buffer.at(index, 3742);
            return false;
        }
    }
    buffer.at(left - 1, 3747) = '1';
    return true;
}

template<class Writer> void writeNegativeScientificIntegerTo(Writer& writer, FormatInteger value,
    TypedIntegerComponents parts, const FormatSpec& spec) {
    IntegerVoidFormatBuffer buffer;
    auto position = buffer.bytes.size() - parts.digits.size() - 1;
    for (std::size_t index = 0; index < parts.digits.size(); ++index)
        buffer.at(position + 1 + index, 183) = static_cast<unsigned char>(parts.digits[index]);
    auto end = position + static_cast<std::size_t>(spec.precision) + 2;
    if (end <= buffer.bytes.size()) {
        char rounding = '0';
        if (end < buffer.bytes.size()) {
            const auto first = buffer.at(end, 261);
            const unsigned char tie = parts.hexFloat ? '8' : '5';
            const auto zeroTail = [&] {
                for (auto index = end + 1; index < buffer.bytes.size(); ++index)
                    if (buffer.at(index, 264) != '0') return false;
                return true;
            };
            if (first >= tie) rounding = first == tie && zeroTail() ? '5' : '9';
            else rounding = first == '0' && zeroTail() ? '0' : '1';
        }
        if (roundIntegerFormatBuffer(buffer, position + 1, end, rounding, value.negative,
            parts.hexFloat ? (parts.upper ? 'F' : 'f') : '9')) {
            --position;
            --end;
        }
    }
    const char integral = static_cast<char>(buffer.at(position + 1, 288));
    buffer.at(position + 1, 289) = '.';
    auto exponent = buffer.bytes.size() - position - 2;
    if (parts.hexFloat) {
        parts.prefix += parts.upper ? "0X" : "0x";
        exponent *= 4;
    }
    std::array<char, 4> suffix{parts.hexFloat ? (parts.upper ? 'P' : 'p') :
        (spec.spec == 'E' ? 'E' : 'e'), '+', static_cast<char>('0' + exponent / 10),
        static_cast<char>('0' + exponent % 10)};
    const auto fraction = buffer.slice(position + 1, std::min(end, buffer.bytes.size()), 324);
    const auto frame = spec;
    ngWriteAlignedFormatTo(writer, compat::RetainedString::fromBorrowedView(parts.prefix),
        compat::RetainedString::fromBorrowedView(std::string_view(&integral, 1)), fraction,
        compat::RetainedString::fromBorrowedView(std::string_view(suffix.data(), suffix.size())), frame,
        FormatPrecisionType::FractionalDigits);
}

template<class Writer> bool writeTypedIntegerValueTo(Writer& writer, FormatInteger value,
    const FormatSpec& spec) {
    if (spec.spec == 'r') {
        const auto bytes = formatRawBits(value.raw, value.bits, spec.flPlus, spec.flDash);
        for (char byte : bytes) ngTypedFormatPut(writer, byte);
        return true;
    }
    auto parts = typedIntegerComponents(value, spec.spec, spec.flPlus, spec.flSpace);
    if (parts.floatingStyle) {
        // f/F and g/G take the original fixed branch even for negative
        // precision (the g/G length comparison uses unsigned size_t).
        if (spec.precision < 0 && !parts.fixed && !parts.general) {
            writeNegativeScientificIntegerTo(writer, value, std::move(parts), spec);
            return true;
        }
        auto frame = spec;
        if (frame.precision == FormatSpec::UNSPECIFIED)
            frame.precision = static_cast<int>(parts.digits.size() - 1);
        const bool general = parts.general;
        const auto precisionType = general ? FormatPrecisionType::AllDigits : FormatPrecisionType::FractionalDigits;
        std::string grouped, fraction, suffix;
        if (parts.fixed || (general && static_cast<std::size_t>(frame.precision) >= parts.digits.size() - 1)) {
            if (spec.precision == FormatSpec::UNSPECIFIED) frame.precision = 0;
            grouped = std::move(parts.digits);
            fraction = ".";
        } else {
            if (general && frame.precision == 0) frame.precision = 1;
            const auto keep = static_cast<std::size_t>(frame.precision) + (general ? 0 : 1);
            auto exponent = parts.digits.size() - 1;
            if (keep < parts.digits.size()) {
                const auto first = formatHexDigit(parts.digits[keep]);
                const bool tail = std::any_of(parts.digits.begin() + keep + 1, parts.digits.end(),
                    [](char digit) { return digit != '0'; });
                const auto retained = formatHexDigit(parts.digits[keep - 1]);
                const auto tie = parts.base / 2;
                const char roundingClass = first < tie ? '1' : first == tie ? '5' : '9';
                const bool increment = incrementRoundedFormat(value.negative, first != 0 || tail,
                    roundingClass, tail, (retained & 1) != 0);
                parts.digits.resize(keep);
                if (increment) {
                    bool carry = true;
                    for (auto i = parts.digits.size(); i && carry; --i) {
                        const auto digit = formatHexDigit(parts.digits[i - 1]);
                        carry = digit + 1 == parts.base;
                        parts.digits[i - 1] = carry ? '0' : formatHexCharacter(digit + 1, parts.upper);
                    }
                    if (carry) {
                        parts.digits.insert(parts.digits.begin(), '1');
                        parts.digits.resize(keep);
                        ++exponent;
                    }
                }
            }
            grouped.assign(parts.digits.data(), 1);
            fraction = "." + parts.digits.substr(1);
            if (general && !spec.flHash)
                while (!fraction.empty() && (fraction.back() == '0' || fraction.back() == '.'))
                    fraction.pop_back();
            if (parts.hexFloat) {
                parts.prefix += parts.upper ? "0X" : "0x";
                exponent *= 4;
            }
            suffix = parts.hexFloat ? (parts.upper ? "P+" : "p+") :
                (spec.spec == 'E' || spec.spec == 'G' ? "E+" : "e+");
            suffix.push_back(static_cast<char>('0' + exponent / 10));
            suffix.push_back(static_cast<char>('0' + exponent % 10));
        }
        ngWriteAlignedFormatTo(writer, compat::RetainedString::fromBorrowedView(parts.prefix),
            compat::RetainedString::fromBorrowedView(grouped), compat::RetainedString::fromBorrowedView(fraction),
            compat::RetainedString::fromBorrowedView(suffix), frame, precisionType);
        return true;
    }
    if (spec.flHash && parts.magnitude && parts.base == 16)
        parts.prefix = spec.spec == 'X' ? "0X" : "0x";
    if (spec.flHash && parts.magnitude && parts.base == 8 &&
        (parts.digits.size() >= static_cast<std::size_t>(spec.precision) ||
            spec.precision == FormatSpec::UNSPECIFIED))
        parts.prefix = "0";
    const compat::RetainedStringSlice prefix(compat::RetainedString::fromBorrowedView(parts.prefix));
    const compat::RetainedStringSlice grouped(compat::RetainedString::fromBorrowedView(parts.digits));
    const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
    ngWriteAlignedFormatTo(writer, prefix, grouped, empty, empty, spec, FormatPrecisionType::Integer);
    return true;
}

template<class Writer> void writeTypedTextValueTo(Writer& writer, compat::RetainedStringSlice text,
    const FormatSpec& spec, bool slicePrecision) {
    if (slicePrecision && static_cast<std::size_t>(spec.precision) < text.size())
        text = text.slice(0, static_cast<std::size_t>(spec.precision));
    auto aligned = spec;
    aligned.flZero = false;
    const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
    ngWriteAlignedFormatTo(writer, empty, empty, empty, std::move(text), aligned);
}

template<class Writer> bool writeTypedRangeValueTo(Writer& writer, const FormatRange& range,
    const FormatSpec& spec);
template<class Writer> bool writeTypedElementValueTo(Writer& writer, const TypedFormatArgument& argument,
    const FormatSpec& spec);

template<class Writer, FormatFloating T> bool writeTypedFloatingValueTo(Writer& writer, T value,
    const FormatSpec& spec) {
    const auto conversion = spec.spec;
    if (conversion == 'r') {
        std::string bytes;
        if constexpr (FormatExtendedReal<T>) {
            bytes.assign(reinterpret_cast<const char*>(&value), sizeof(value));
            const bool swap = (std::endian::native == std::endian::little && spec.flPlus) ||
                (std::endian::native == std::endian::big && spec.flDash);
            if (swap) std::reverse(bytes.begin(), bytes.end());
        } else {
            using Bits = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;
            bytes = formatRawBits(static_cast<std::uint64_t>(std::bit_cast<Bits>(value)),
                sizeof(T) * CHAR_BIT, spec.flPlus, spec.flDash);
        }
        for (char byte : bytes) ngTypedFormatPut(writer, byte);
        return true;
    }
    FormatSpec frame = spec;
    frame.spec = conversion == 's' ? 'g' : conversion;
    if (std::string_view("eEfFgGaA").find(frame.spec) == std::string_view::npos)
        formatFailure(std::string("incompatible format character for floating point argument: %") + conversion,
            "std/format/internal/write.d", 602);
    DecimalFormatSpec numeric;
    numeric.conversion = frame.spec;
    numeric.plus = frame.flPlus;
    numeric.space = frame.flSpace;
    numeric.alternate = frame.flHash;
    if (frame.spec == 'a' || frame.spec == 'A') {
        auto parts = frame.precision == FormatSpec::UNSPECIFIED ? makeHexFormatComponents(value, numeric, false) :
            makeSignedHexFormatParts(value, numeric, frame.precision);
        writeFloatingFormatPartsTo(writer, std::move(parts), std::move(frame));
    } else {
        const auto precision = frame.precision == FormatSpec::UNSPECIFIED ? 6 : frame.precision;
        auto parts = makeSignedDecimalFormatParts(value, numeric, precision);
        writeFloatingFormatPartsTo(writer, std::move(parts), std::move(frame));
    }
    return true;
}

template<class Unit> class FormatWideUnitCursor {
    std::basic_string_view<Unit> text_;
    std::size_t index_ = 0;
public:
    explicit FormatWideUnitCursor(std::basic_string_view<Unit> text) : text_(text) {}
    bool empty() const { return index_ == text_.size(); }
    char32_t front() const { return static_cast<char32_t>(text_[index_]); }
    void popFront() { ++index_; }
};

template<class Unit> struct FormatCharacterRangeSuffix {
    std::basic_string_view<Unit> text;
    using Cursor = std::conditional_t<sizeof(Unit) == 4, FormatWideUnitCursor<Unit>,
        compat::phobos::detail::UnicodeCursor<std::basic_string_view<Unit>, Unit>>;
    std::int64_t displayWidth() const {
        if (std::all_of(text.begin(), text.end(), [](Unit unit) {
            return static_cast<std::make_unsigned_t<Unit>>(unit) <= 0x7f;
        })) return static_cast<std::int64_t>(text.size());
        Cursor cursor(text);
        std::int64_t width = 0;
        while (!cursor.empty()) {
            ++width;
            auto state = FormatGraphemeState::Start;
            bool finished = false;
            while (!finished && !cursor.empty()) {
                const auto scalar = cursor.front();
                for (;;) {
                    const auto step = stepFormatGrapheme(state, scalar);
                    if (step == FormatGraphemeStep::Redo) continue;
                    if (step != FormatGraphemeStep::Exclude) cursor.popFront();
                    finished = step != FormatGraphemeStep::Advance;
                    break;
                }
            }
        }
        return width;
    }
    template<class Writer> void putTo(Writer& writer) const {
        if constexpr (requires { writer.put(text); }) writer.put(text);
        else if constexpr (std::invocable<Writer&, decltype(text)>) std::invoke(writer, text);
        else if constexpr (sizeof(Unit) == 1) {
            for (auto value : text) ngTypedFormatPut(writer, static_cast<char>(value));
        } else {
            Cursor cursor(text);
            while (!cursor.empty()) {
                const auto value = cursor.front();
                cursor.popFront();
                putFormatCodePoint(writer, value);
            }
        }
    }
};

template<class Unit> struct FormatScalarSuffix {
    Unit value;
    char32_t decoded() const {
        if constexpr (sizeof(Unit) == 4) return static_cast<char32_t>(value);
        else {
            const std::basic_string_view<Unit> text(&value, 1);
            compat::phobos::detail::UnicodeCursor<std::basic_string_view<Unit>, Unit> cursor(text);
            return cursor.front();
        }
    }
    std::int64_t displayWidth() const { (void)decoded(); return 1; }
    template<class Writer> void putTo(Writer& writer) const {
        const std::basic_string_view<Unit> text(&value, 1);
        if constexpr (requires { writer.put(text); }) writer.put(text);
        else if constexpr (std::invocable<Writer&, decltype(text)>) std::invoke(writer, text);
        else putFormatCodePoint(writer, decoded());
    }
};

template<class Writer> bool writeTypedScalarValueTo(Writer& writer, const TypedFormatArgument& argument,
    const FormatSpec& spec) {
    if (spec.width == FormatSpec::DYNAMIC || spec.precision == FormatSpec::DYNAMIC ||
        spec.separators == FormatSpec::DYNAMIC || spec.dynamicSeparatorChar)
        formatFailure("Dynamic argument not allowed for `formatValue`", "std/format/write.d", 1259);
    return std::visit([&](const auto& value) -> bool {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<T, FormatInteger>) return writeTypedIntegerValueTo(writer, value, spec);
        else if constexpr (std::same_as<T, FormatPointer>) {
            if (spec.spec == 's' && value.address == 0) {
                writeTypedTextValueTo(writer, compat::RetainedStringSlice(
                    compat::RetainedString::fromStaticLiteral("null")), spec, false);
                return true;
            }
            if (spec.spec == 's') {
                auto hexadecimal = spec;
                hexadecimal.spec = 'X';
                return writeTypedIntegerValueTo(writer,
                    FormatInteger{static_cast<std::uint64_t>(value.address), 64, false, false}, hexadecimal);
            }
            if (spec.spec != 'x' && spec.spec != 'X')
                formatFailure("Expected one of %s, %x or %X for pointer type.",
                    "std/format/internal/write.d", 2977);
            return writeTypedIntegerValueTo(writer,
                FormatInteger{static_cast<std::uint64_t>(value.address), 64, false, false}, spec);
        } else if constexpr (std::same_as<T, bool>) {
            if (spec.spec != 's')
                return writeTypedIntegerValueTo(writer, FormatInteger{value ? 1u : 0u, 8, false, true}, spec);
            writeTypedTextValueTo(writer, compat::RetainedStringSlice(value
                ? compat::RetainedString::fromStaticLiteral("true")
                : compat::RetainedString::fromStaticLiteral("false")), spec, false);
            return true;
        } else if constexpr (std::same_as<T, FormatCharacter>) {
            if (spec.spec != 's' && spec.spec != 'c') return writeTypedIntegerValueTo(writer,
                FormatInteger{static_cast<std::uint64_t>(value.value), value.bits, false}, spec);
            auto frame = spec;
            frame.flZero = false;
            const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
            if (value.bits == 8) {
                const char character = static_cast<char>(value.value);
                writeTypedTextValueTo(writer, compat::RetainedString::fromBorrowedView(
                    std::string_view(&character, 1)), spec, false);
            } else if (value.bits == 16) {
                ngWriteAlignedFormatWithSuffixTo(writer, empty, empty, empty,
                    FormatScalarSuffix<char16_t>{static_cast<char16_t>(value.value)}, frame);
            } else {
                ngWriteAlignedFormatWithSuffixTo(writer, empty, empty, empty,
                    FormatScalarSuffix<char32_t>{value.value}, frame);
            }
            return true;
        } else if constexpr (std::same_as<T, FormatEnum>) {
            const auto base = std::visit([](const auto& item) -> TypedFormatArgument { return item; }, value.base);
            if (spec.spec != 's') return writeTypedScalarValueTo(writer, base, spec);
            if (value.memberName) {
                writeTypedTextValueTo(writer, *value.memberName, spec, true);
                return true;
            }
            std::string text = allocateAggregateText([&] {
                return "cast(" + std::string(value.typeName.view()) + ")";
            });
            TypedFormatWriter inner(text);
            auto innerSpec = spec;
            innerSpec.width = 0;
            if (!writeTypedScalarValueTo(inner, base, innerSpec)) {
                const auto raw = formatTypedArgument(base, ngParsedFormatSpec(innerSpec));
                allocateAggregateText([&] { text += raw; });
            }
            writeTypedTextValueTo(writer,
                compat::RetainedStringSlice(compat::RetainedString(std::move(text))), spec, false);
            return true;
        } else if constexpr (std::same_as<T, compat::RetainedStringSlice>) {
            if (spec.spec == 'r') {
                // The source dynamic-array raw path calls formatValue again on
                // each element, observing format-state changes between puts.
                for (unsigned char byte : value.view()) {
                    const TypedFormatArgument element = FormatCharacter{byte, 8};
                    if (!writeTypedScalarValueTo(writer, element, spec)) {
                        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(
                            formatTypedArgument(element, ngParsedFormatSpec(spec)))));
                    }
                }
                return true;
            }
            if (spec.spec == '(') {
                using Cursor = compat::phobos::detail::UnicodeCursor<compat::RetainedStringSlice, char>;
                const auto cursor = std::allocate_shared<Cursor>(
                    nicxlive::core::d_runtime::SourceAllocator<Cursor>{}, value);
                FormatRange characters;
                characters.length = value.size();
                try {
                    characters.read = [cursor](std::size_t) -> TypedFormatArgument {
                        return FormatCharacter{cursor->front(), 32};
                    };
                    characters.empty = [cursor] { return cursor->empty(); };
                    characters.popFront = [cursor] { cursor->popFront(); };
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
                return writeTypedRangeValueTo(writer, characters, spec);
            }
            if (spec.spec != 's') formatFailure(allocateAggregateText([&] {
                return std::string("Incorrect format specifier for range: %") + spec.spec;
            }), "std/format/internal/write.d", 1588);
            writeTypedTextValueTo(writer, value, spec, true);
            return true;
        } else if constexpr (std::same_as<T, std::shared_ptr<FormatRange>>) {
            if (!value) throw std::invalid_argument("Missing retained format range");
            return writeTypedRangeValueTo(writer, *value, spec);
        } else if constexpr (std::same_as<T, FormatObject>) {
            if (!value.acceptsFormatSpec && spec.spec != 's') {
                if (value.sourceTypeName.empty())
                    throw std::logic_error("Source custom format type metadata is unavailable");
                formatFailure(allocateAggregateText([&] {
                    return "Expected '%s' format specifier for type '" +
                        std::string(value.sourceTypeName.view()) + "'";
                }), "std/format/internal/write.d", 2862);
            }
            if (value.write) {
                if constexpr (std::same_as<std::remove_cvref_t<Writer>, FormatWriterRef>) value.write(writer, spec);
                else {
                    FormatWriterRef reference(writer);
                    value.write(reference, spec);
                }
            } else if (value.format) ngTypedFormatPut(writer, compat::RetainedStringSlice(value.format(spec)));
            else ngTypedFormatPut(writer, compat::RetainedStringSlice(value.toString()));
            return true;
        } else if constexpr (std::same_as<T, std::shared_ptr<FormatAggregate>>) {
            if (!value || value->sourceTypeName.empty())
                throw std::logic_error("Source aggregate field metadata is unavailable");
            if (spec.spec != 's') formatFailure(allocateAggregateText([&] {
                return "Expected '%s' format specifier for type '" + value->sourceTypeName + "'";
            }), "std/format/internal/write.d", 2862);
            const auto put = [&](std::string text) {
                ngTypedFormatPut(writer, compat::RetainedStringSlice(retainFormatMessage(std::move(text))));
            };
            if (value->isUnion) put(allocateAggregateText([&] { return value->sourceTypeName; }));
            else {
                put(allocateAggregateText([&] { return value->sourceTypeName + "("; }));
                for (std::size_t i = 0; i < value->fields.size(); ++i) {
                    const auto& field = value->fields[i];
                    if (field.hiddenContext) continue;
                    if (i && value->fields[i - 1].offset == field.offset) {
                        auto text = allocateAggregateText([&] { return ", " + std::string(field.name); });
                        if (i + 1 == value->fields.size() || value->fields[i + 1].offset != field.offset)
                            allocateAggregateText([&] { text += '}'; });
                        put(std::move(text));
                    } else if (i + 1 < value->fields.size() && value->fields[i + 1].offset == field.offset) {
                        put(allocateAggregateText([&] {
                            return std::string(i ? ", " : "") + "#{overlap " + std::string(field.name);
                        }));
                    } else {
                        if (i) put(allocateAggregateText([] { return std::string(", "); }));
                        const auto argument = value->read(i);
                        if (!writeTypedElementValueTo(writer, argument, spec)) {
                            const auto text = formatTypedElement(argument, ngParsedFormatSpec(spec));
                            put(allocateAggregateText([&] { return text; }));
                        }
                    }
                }
                put(allocateAggregateText([] { return std::string(")"); }));
            }
            return true;
        } else if constexpr (FormatFloating<T>) {
            return writeTypedFloatingValueTo(writer, value, spec);
        } else return false;
    }, argument);
}

template<class Writer> void writeQuotedCodePointTo(Writer& writer, char32_t value, char quote) {
    if (isFormatGraphical(value)) {
        if (value == static_cast<char32_t>(quote) || value == '\\') ngTypedFormatPut(writer, '\\');
        putFormatCodePoint(writer, value);
        return;
    }
    constexpr char controls[] = {'\n', '\r', '\t', '\a', '\b', '\f', '\v', '\0'};
    constexpr char escapes[] = {'n', 'r', 't', 'a', 'b', 'f', 'v', '0'};
    for (std::size_t i = 0; i < sizeof(controls); ++i) {
        if (value == static_cast<unsigned char>(controls[i])) {
            ngTypedFormatPut(writer, '\\');
            ngTypedFormatPut(writer, escapes[i]);
            return;
        }
    }
    FormatSpec hexadecimal;
    hexadecimal.spec = 'X';
    hexadecimal.flZero = true;
    if (value <= 0xff) {
        hexadecimal.width = 2;
        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("\\x")));
    } else if (value <= 0xffff) {
        hexadecimal.width = 4;
        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("\\u")));
    } else {
        hexadecimal.width = 8;
        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("\\U")));
    }
    (void)writeTypedIntegerValueTo(writer, FormatInteger{static_cast<std::uint32_t>(value), 32, false, false},
        hexadecimal);
}

template<class Range> class FormatDcharCursor {
    Range input_;
    std::size_t index_ = 0;
public:
    explicit FormatDcharCursor(Range input) : input_(std::move(input)) {}
    bool empty() const { return index_ == input_.size(); }
    char32_t front() const { return validateFormatDchar(input_[index_]); }
    void popFront() { ++index_; }
};

template<class Writer, class Range, class Unit>
void writeQuotedCharacterSequenceTo(Writer& writer, Range text) {
    using Cursor = std::conditional_t<sizeof(Unit) == 4, FormatDcharCursor<Range>,
        compat::phobos::detail::UnicodeCursor<Range, Unit>>;
    try {
        bool valid = true;
        Cursor validation(text);
        while (!validation.empty()) {
            const auto value = validation.front();
            validation.popFront();
            if (value == 0xfffe || value == 0xffff) { valid = false; break; }
        }
        if (valid) {
            ngTypedFormatPut(writer, '"');
            Cursor output(text);
            while (!output.empty()) {
                const auto value = output.front();
                output.popFront();
                writeQuotedCodePointTo(writer, value, '"');
            }
            ngTypedFormatPut(writer, '"');
            return;
        }
    } catch (const compat::UTFException&) {
        // The Source try block includes writer calls as well as UTF decoding.
    }
    ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("[")));
    const compat::RetainedStringSlice pattern(sizeof(Unit) == 1
        ? compat::RetainedString::fromStaticLiteral("cast(char) 0x%X")
        : sizeof(Unit) == 2 ? compat::RetainedString::fromStaticLiteral("cast(wchar) 0x%X")
        : compat::RetainedString::fromStaticLiteral("cast(dchar) 0x%X"));
    const auto prefix = pattern.slice(0, pattern.size() - 2);
    const auto trailing = pattern.slice(pattern.size(), pattern.size());
    FormatSpec hexadecimal;
    hexadecimal.spec = 'X';
    for (std::size_t i = 0; i < text.size(); ++i) {
        ngTypedFormatPut(writer, prefix);
        const auto value = [&] {
            if constexpr (requires { text[i]; }) return text[i];
            else return text.view()[i];
        }();
        using Unsigned = std::make_unsigned_t<Unit>;
        (void)writeTypedIntegerValueTo(writer,
            FormatInteger{static_cast<Unsigned>(value), sizeof(Unit) * CHAR_BIT, false, false}, hexadecimal);
        ngTypedFormatPut(writer, trailing);
        if (i + 1 < text.size()) ngTypedFormatPut(writer,
            compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(", ")));
    }
    ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("]")));
}

template<class Writer> bool writeTypedElementValueTo(Writer& writer, const TypedFormatArgument& argument,
    const FormatSpec& spec) {
    if (spec.spec == 's') {
        if (const auto text = std::get_if<compat::RetainedStringSlice>(&argument)) {
            writeQuotedCharacterSequenceTo<Writer, compat::RetainedStringSlice, char>(writer, *text);
            return true;
        }
        if (const auto range = std::get_if<std::shared_ptr<FormatRange>>(&argument); range && *range) {
            const FormatCodeUnitView view{range->get(), (*range)->length};
            switch ((*range)->characterBits) {
            case 8: writeQuotedCharacterSequenceTo<Writer, FormatCodeUnitView, char>(writer, view); return true;
            case 16: writeQuotedCharacterSequenceTo<Writer, FormatCodeUnitView, char16_t>(writer, view); return true;
            case 32: writeQuotedCharacterSequenceTo<Writer, FormatCodeUnitView, char32_t>(writer, view); return true;
            }
        }
        if (const auto character = std::get_if<FormatCharacter>(&argument)) {
            ngTypedFormatPut(writer, '\'');
            writeQuotedCodePointTo(writer, character->value, '\'');
            ngTypedFormatPut(writer, '\'');
            return true;
        }
    }
    return writeTypedScalarValueTo(writer, argument, spec);
}

template<class Writer> bool writeTypedRangeValueTo(Writer& writer, const FormatRange& range,
    const FormatSpec& spec) {
    if (range.characterBits && spec.spec == 's') {
        return std::visit([&](const auto& view) -> bool {
            using View = std::remove_cvref_t<decltype(view)>;
            if constexpr (std::same_as<View, std::monostate>) return false;
            else {
                const auto length = std::min(view.size(), static_cast<std::size_t>(spec.precision));
                // The original text overload copies flags after slicing units.
                auto frame = spec;
                frame.flZero = false;
                const compat::RetainedStringSlice empty(compat::RetainedString::emptyLiteral());
                const View sliced(view.data(), length);
                ngWriteAlignedFormatWithSuffixTo(writer, empty, empty, empty,
                    FormatCharacterRangeSuffix<typename View::value_type>{sliced}, frame);
                return true;
            }
        }, range.characterView);
    }
    if (range.characterBits == 8 && spec.spec == '(') {
        const auto decoded = decodedFormatRange<char>(range);
        return writeTypedRangeValueTo(writer, decoded, spec);
    }
    if (range.characterBits == 16 && spec.spec == '(') {
        const auto decoded = decodedFormatRange<char16_t>(range);
        return writeTypedRangeValueTo(writer, decoded, spec);
    }
    std::size_t index = 0;
    const auto empty = [&] { return range.empty ? range.empty() : index == range.length; };
    const auto popFront = [&] {
        if (range.popFront) range.popFront();
        ++index;
    };
    const auto writeValue = [&](const FormatSpec& current, bool quoted) {
        const auto argument = range.read(index);
        const bool written = quoted ? writeTypedElementValueTo(writer, argument, current) :
            writeTypedScalarValueTo(writer, argument, current);
        if (!written) {
            const auto parsed = ngParsedFormatSpec(current);
            const auto text = quoted ? formatTypedElement(argument, parsed) : formatTypedArgument(argument, parsed);
            ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(text)));
        }
    };
    if (spec.spec == 's') {
        ngTypedFormatPut(writer, compat::RetainedStringSlice(FormatSpec::seqBefore));
        bool first = true;
        while (!empty()) {
            if (!first) ngTypedFormatPut(writer, compat::RetainedStringSlice(FormatSpec::seqSeparator));
            writeValue(spec, true);
            popFront();
            first = false;
        }
        ngTypedFormatPut(writer, compat::RetainedStringSlice(FormatSpec::seqAfter));
        return true;
    }
    if (spec.spec == 'r') {
        while (!empty()) {
            writeValue(spec, false);
            popFront();
        }
        return true;
    }
    if (spec.spec == '(') {
        if (empty()) return true;
        for (;;) {
            FormatSpec nested(spec.nested);
            for (;;) {
                if (!nested.writeUpToNextSpec(writer)) break;
                writeValue(nested, !spec.flDash);
                if (nested.trailing.view().find('%') == std::string_view::npos) break;
            }
            if (!spec.sep.isNull()) {
                ngTypedFormatPut(writer, nested.trailing);
                popFront();
                if (empty()) break;
                ngTypedFormatPut(writer, spec.sep);
            } else {
                popFront();
                if (empty()) break;
                ngTypedFormatPut(writer, nested.trailing);
            }
        }
        return true;
    }
    formatFailure(allocateAggregateText([&] {
        return std::string("Incorrect format specifier for range: %") + spec.spec;
    }), "std/format/internal/write.d", 1588);
}
} // namespace detail

namespace detail {
template<class Sink>
std::size_t formatTypedTo(Sink&& sink, std::string_view pattern, std::span<const TypedFormatArgument> arguments) {
    std::size_t next = 0;
    std::size_t offset = 0;
    bool consumedAll=false;
    while (offset < pattern.size()) {
        if (pattern[offset] != '%') {
            const auto start = offset;
            while (offset < pattern.size() && pattern[offset] != '%') ++offset;
            std::invoke(sink, pattern.substr(start, offset - start));
            continue;
        }
        ++offset;
        if (offset==pattern.size())
            formatFailure("Unterminated format specifier: \"%\"","std/format/spec.d",262);
        if (offset < pattern.size() && pattern[offset] == '%') {
            const auto start = offset++;
            while (offset < pattern.size() && pattern[offset] != '%') ++offset;
            std::invoke(sink, pattern.substr(start, offset - start));
            continue;
        }
        auto spec = detail::parseTypedFormat(pattern, offset);
        const auto orphan=[&](std::size_t line) {
            if (next==arguments.size() && !spec.argument)
                formatFailure(allocateAggregateText([&] {
                    return std::string("Orphan format specifier: %")+spec.conversion;
                }),"std/format/write.d",line);
        };
        orphan(544);
        const auto field = [&](const detail::DynamicFormatField& dynamic,std::string_view kind) {
            const auto index = dynamic.argument.value_or(next);
            if (index >= arguments.size()) formatFailure(allocateAggregateText([&] {
                return "Missing "+std::string(kind)+" argument";
            }),
                "std/format/internal/write.d",3266);
            const auto value = detail::dynamicFormatInteger(arguments[index],kind,index);
            next = std::max(next, index + 1);
            return value;
        };
        if (spec.dynamicWidth.dynamic) {
            const auto width = field(spec.dynamicWidth,"integer width");
            if (width < 0) {
                spec.left = true;
                // Source int negation wraps int.min; a negative final width
                // contributes no alignment padding, rather than a new error.
                spec.width = width==INT_MIN?0:static_cast<std::size_t>(-width);
            }
            else spec.width = static_cast<std::size_t>(width);
            spec.dynamicWidth={};
        }
        if (spec.dynamicPrecision.dynamic) {
            const auto precision = field(spec.dynamicPrecision,"integer precision");
            if (precision < 0 || precision == INT_MAX - 1) spec.precision.reset();
            else spec.precision = static_cast<std::size_t>(precision);
            spec.dynamicPrecision={};
        }
        if (spec.dynamicGroup) {
            spec.groupSize = field(DynamicFormatField{true, {}},"separator digit width");
            spec.dynamicGroup=false;
        }
        if (spec.dynamicSeparator) {
            if (next >= arguments.size()) formatFailure("Missing separator character argument",
                "std/format/internal/write.d",3266);
            const auto index=next++;
            const auto& separatorArgument = arguments[index];
            auto character = std::get_if<FormatCharacter>(&separatorArgument);
            if (const auto enumeration = std::get_if<FormatEnum>(&separatorArgument))
                character = std::get_if<FormatCharacter>(&enumeration->base);
            if (!character) {
                const auto typeName = formatSourceTypeName(separatorArgument);
                formatFailure(allocateAggregateText([&] {
                    return "separator character expected, not "+typeName+
                        " for argument #"+std::to_string(index+1);
                }),"std/format/internal/write.d",3260);
            }
            spec.separator = character->value;
            spec.dynamicSeparator=false;
        }
        orphan(614);
        const auto first = spec.argument.value_or(next);
        auto last = spec.remainingArguments ? (arguments.empty() ? 0 : arguments.size() - 1) :
            spec.lastArgument.value_or(first);
        if (first >= arguments.size()) {
            if (spec.remainingArguments) { next = 65535; consumedAll=true; continue; }
        }
        const auto positionalFailure=[&] {
            const auto start=spec.argument?*spec.argument+1:0;
            const auto end=spec.consumeThrough.value_or(0);
            auto message=allocateAggregateText([&] {
                std::string text="Positional specifier %"+std::to_string(start);
                if (start!=end) text+=":"+std::to_string(end);
                text+='$'; text+=spec.conversion; text+=" index exceeds "+std::to_string(arguments.size());
                return text;
            });
            formatFailure(std::move(message),"std/format/write.d",start==end?659:663);
        };
        // A reversed explicit range formats its first argument once, as Phobos.
        const auto consumed = spec.remainingArguments ? 65535 : spec.consumeThrough.value_or(first + 1);
        last = std::max(first, last);
        const auto publicSpec = ngPublicFormatSpec(spec);
        for (auto index = first; index <= last; ++index) {
            // Preserve successful prefix formatter effects before a range-end
            // failure, rather than rejecting the full range before formatting.
            if (index >= arguments.size()) {
                if (spec.remainingArguments) { consumedAll=true; break; }
                positionalFailure();
            }
            if (!writeTypedScalarValueTo(sink, arguments[index], publicSpec)) {
                const auto formatted = detail::formatTypedArgument(arguments[index], spec);
                std::invoke(sink, std::string_view(formatted));
            }
        }
        next = std::max(next, consumed);
    }
    return consumedAll?arguments.size():next;
}
inline std::string copyFormatTypeName(std::string_view name) {
    try {
        return std::string(name);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline std::string formatTypedBody(std::string_view pattern, std::span<const TypedFormatArgument> arguments) {
    std::string result;
    const auto consumed=formatTypedTo([&](std::string_view bytes) {
        try {
            result.append(bytes);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }, pattern, arguments);
    if (consumed!=arguments.size()) {
        std::string message;
        try {
            message="Orphan format arguments: args["+std::to_string(consumed)+".."+
                std::to_string(arguments.size())+"]";
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        formatFailure(std::move(message),"std/format/package.d",785);
    }
    return result;
}
} // namespace detail

std::string ngFormatTyped(std::string_view pattern, std::span<const TypedFormatArgument> arguments);
void ngFormatTypedTo(const TypedFormatSink& sink, std::string_view pattern,
    std::span<const TypedFormatArgument> arguments);

template<class Writer, class T> void ngFormatValueTo(Writer& writer, T& value, const FormatSpec& spec);
template<class T, class Member> struct FormatAggregateMember {
    std::string_view name;
    std::size_t offset;
    Member T::* member;
    bool hiddenContext = false;
    template<class Instance> decltype(auto) read(Instance& value) const { return value.*member; }
};
template<class T, class Member> constexpr auto ngFormatAggregateMember(
    std::string_view name, std::size_t offset, Member T::* member, bool hiddenContext = false) {
    return FormatAggregateMember<T, Member>{name, offset, member, hiddenContext};
}
namespace detail {
template<std::size_t I = 0, class Tuple, class T, class Writer>
void formatAggregateElementTo(const Tuple& fields, T& value, std::size_t index,
        Writer& writer, const FormatSpec& spec) {
    if constexpr (I == std::tuple_size_v<Tuple>) throw std::out_of_range("Aggregate metadata field index");
    else {
        if (index == I) {
            auto& field = std::get<I>(fields).read(value);
            using Field = std::remove_reference_t<decltype(field)>;
            if constexpr (HasCustomFormat<Field> || HasFormatObjectTraits<Field> ||
                HasAggregateFormatMetadata<std::remove_cv_t<Field>> ||
                compat::SourceFixedArray<std::remove_cv_t<Field>>::value) ngFormatValueTo(writer, field, spec);
            else {
                const auto argument = ngFormatArgument(field);
                if (writeTypedElementValueTo(writer, argument, spec)) return;
                const auto text = formatTypedElement(argument, ngParsedFormatSpec(spec));
                ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(text)));
            }
        } else formatAggregateElementTo<I + 1>(fields, value, index, writer, spec);
    }
}
template<std::size_t I = 0, class Tuple, class T>
TypedFormatArgument formatAggregateRead(const Tuple& fields, T& value, std::size_t index,
        const std::shared_ptr<void>& owner) {
    if constexpr (I == std::tuple_size_v<Tuple>) throw std::out_of_range("Aggregate metadata field index");
    else {
        if (index == I) {
            auto& field = std::get<I>(fields).read(value);
            if constexpr (compat::SourceFixedArray<std::remove_cvref_t<decltype(field)>>::value)
                return ngFormatFixedArrayReferenceArgument(field, owner);
            else if constexpr (HasCustomFormat<std::remove_reference_t<decltype(field)>> ||
                    HasAggregateFormatMetadata<std::remove_cvref_t<decltype(field)>>)
                return ngFormatValueReferenceArgument(field, owner);
            else return ngFormatArgument(field);
        }
        return formatAggregateRead<I + 1>(fields, value, index, owner);
    }
}
}
template<class T> TypedFormatArgument ngAggregateFormatArgument(T value) {
    auto aggregate = std::allocate_shared<FormatAggregate>(
        nicxlive::core::d_runtime::SourceAllocator<FormatAggregate>{});
    aggregate->sourceTypeName = FormatSourceTypeName<T>::get();
    aggregate->isUnion = formatUnionMetadata<T>;
    if constexpr (!formatUnionMetadata<T>) {
        constexpr auto fields = FormatAggregateTraits<T>::fields();
        const auto appendField = [&](const auto& field) {
            const FormatAggregateField metadata{field.name, field.offset, field.hiddenContext};
            try { aggregate->fields.push_back(metadata); }
            catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        };
        std::apply([&](const auto&... field) {
            (appendField(field), ...);
        }, fields);
        // Retain the actual value; no fabricated or eagerly copied field list.
        aggregate->read = [fields, value = std::allocate_shared<T>(
                nicxlive::core::d_runtime::SourceAllocator<T>{}, std::move(value))](std::size_t index) {
            return detail::formatAggregateRead(fields, *value, index, value);
        };
    }
    return aggregate;
}

template<class Writer, class T> void ngFormatValueTo(Writer& writer, T& value, const FormatSpec& spec) {
    if (spec.width == FormatSpec::DYNAMIC || spec.precision == FormatSpec::DYNAMIC ||
        spec.separators == FormatSpec::DYNAMIC || spec.dynamicSeparatorChar)
        detail::formatFailure("Dynamic argument not allowed for `formatValue`", "std/format/write.d", 1259);
    constexpr bool writerSpec = requires { value.toString(writer, spec); };
    constexpr bool writerOnly = requires { value.toString(writer); };
    constexpr bool retainedSpec = requires(RetainedFormatSink& sink) { value.toString(sink, spec); };
    constexpr bool legacySpec = requires(TypedFormatSink& sink) { value.toString(sink, spec); };
    constexpr bool retainedString = requires(RetainedFormatSink& sink, const compat::RetainedString& text) {
        value.toString(sink, text);
    };
    constexpr bool legacyString = requires(TypedFormatSink& sink, const compat::RetainedString& text) {
        value.toString(sink, text);
    };
    constexpr bool retainedOnly = requires(RetainedFormatSink& sink) { value.toString(sink); };
    constexpr bool legacyOnly = requires(TypedFormatSink& sink) { value.toString(sink); };
    constexpr bool hasMethod = writerSpec || writerOnly || retainedSpec || legacySpec || retainedString ||
        legacyString || retainedOnly || legacyOnly ||
        requires(TypedFormatWriter& output) { output.put(value.toString()); };
    if constexpr (hasMethod) {
        constexpr bool selectedSpec = writerSpec || (!writerOnly && (retainedSpec || retainedString ||
            (!retainedOnly && (legacySpec || legacyString))));
        if constexpr (!selectedSpec) {
            if (spec.spec != 's') {
                const auto name = FormatSourceTypeName<std::remove_cvref_t<T>>::get();
                if (name.empty()) throw std::logic_error("Source custom format type metadata is unavailable");
                detail::formatFailure(detail::allocateAggregateText([&] {
                    return "Expected '%s' format specifier for type '" + name + "'";
                }), "std/format/internal/write.d", 2862);
            }
        }
        if constexpr (writerSpec) value.toString(writer, spec);
        else if constexpr (writerOnly) value.toString(writer);
        else if constexpr (retainedSpec || retainedString || retainedOnly) {
            RetainedFormatSink retainedSink = [&](compat::RetainedStringSlice text) {
                ngTypedFormatPut(writer, text);
            };
            if constexpr (retainedSpec) value.toString(retainedSink, spec);
            else if constexpr (retainedString) value.toString(retainedSink, spec.getCurFmtStr());
            else value.toString(retainedSink);
        } else if constexpr (legacySpec || legacyString || legacyOnly) {
            auto legacySink = detail::allocateAggregateText([&] {
                return TypedFormatSink([&](std::string_view text) {
                    ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(text)));
                });
            });
            if constexpr (legacySpec) value.toString(legacySink, spec);
            else if constexpr (legacyString) value.toString(legacySink, spec.getCurFmtStr());
            else value.toString(legacySink);
        } else ngTypedFormatPut(writer, ngFormatStringResult(value.toString()));
    } else if constexpr (HasQualifiedClassFormat<T>) {
        // Phobos's qualified class path uses the static unqualified ClassInfo
        // name when that qualified receiver has no callable toString. Never
        // infer T.stringof or a source qualifier from a native RTTI spelling.
        if (spec.spec != 's') {
            const auto name = FormatSourceTypeName<T>::get();
            if (name.empty()) throw std::logic_error("Source qualified class format type metadata is unavailable");
            detail::formatFailure("Expected '%s' format specifier for type '" + name + "'",
                "std/format/internal/write.d", 2862);
        }
        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("const(")));
        ngTypedFormatPut(writer, compat::RetainedStringSlice(
            nicxlive::core::d_runtime::ngObjectClassName(typeid(std::remove_const_t<T>))));
        ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(")")));
    } else if constexpr (compat::SourceFixedArray<std::remove_cv_t<T>>::value) {
        const auto argument = ngFormatFixedArrayReferenceArgument(value);
        if (detail::writeTypedScalarValueTo(writer, argument, spec)) return;
        auto text = detail::formatTypedArgument(argument, ngParsedFormatSpec(spec));
        ngTypedFormatPut(writer, compat::RetainedStringSlice(detail::retainFormatMessage(std::move(text))));
    } else if constexpr (HasAggregateFormatMetadata<std::remove_cvref_t<T>>) {
        using Traits = FormatAggregateTraits<std::remove_cvref_t<T>>;
        const auto name = FormatSourceTypeName<std::remove_cvref_t<T>>::get();
        if (name.empty()) throw std::logic_error("Source aggregate type metadata is unavailable");
        if (spec.spec != 's') detail::formatFailure(detail::allocateAggregateText([&] {
            return "Expected '%s' format specifier for type '" + name + "'";
        }), "std/format/internal/write.d", 2862);
        const auto put = [&](std::string text) {
            ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(std::move(text))));
        };
        if constexpr (formatUnionMetadata<std::remove_cvref_t<T>>) put(detail::copyFormatTypeName(name));
        else {
            constexpr auto fields = Traits::fields();
            constexpr auto metadata = std::apply([](const auto&... field) {
                return std::array<FormatAggregateField, sizeof...(field)>{
                    FormatAggregateField{field.name, field.offset, field.hiddenContext}...};
            }, fields);
            put(detail::allocateAggregateText([&] { return name + "("; }));
            for (std::size_t i = 0; i < metadata.size(); ++i) {
                const auto& field = metadata[i];
                if (field.hiddenContext) continue;
                if (i && metadata[i - 1].offset == field.offset) {
                    put(detail::allocateAggregateText([&] {
                        return std::string(", ") + std::string(field.name) +
                            ((i + 1 == metadata.size() || metadata[i + 1].offset != field.offset) ? "}" : "");
                    }));
                } else if (i + 1 < metadata.size() && metadata[i + 1].offset == field.offset) {
                    put(detail::allocateAggregateText([&] {
                        return std::string(i ? ", " : "") + "#{overlap " + std::string(field.name);
                    }));
                } else {
                    if (i) put(detail::copyFormatTypeName(", "));
                    detail::formatAggregateElementTo(fields, value, i, writer, spec);
                }
            }
            ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(")")));
        }
    } else {
        if constexpr (std::is_const_v<T> && HasCustomFormat<std::remove_const_t<T>> && !HasCustomFormat<T>)
            throw std::logic_error("Source const aggregate field metadata is unavailable");
        else {
            const auto argument = ngFormatArgument(value);
            if (detail::writeTypedScalarValueTo(writer, argument, spec)) return;
            const auto text = detail::formatTypedArgument(argument, ngParsedFormatSpec(spec));
            ngTypedFormatPut(writer, compat::RetainedStringSlice(compat::RetainedString(text)));
        }
    }
}
template<class T> TypedFormatArgument ngCustomFormatArgument(T value) {
    FormatObject result;
    result.sourceTypeName = detail::retainFormatMessage(FormatSourceTypeName<T>::get());
    // The real selected overload validates its own accepted specifier below.
    result.acceptsFormatSpec = true;
    auto retained = std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{}, std::move(value));
    result.format = [value = retained](const FormatSpec& spec) {
        std::string text;
        TypedFormatWriter writer(text);
        ngFormatValueTo(writer, *value, spec);
        return detail::retainFormatMessage(std::move(text));
    };
    if constexpr (HasCustomFormatFor<T, FormatWriterRef>) {
        result.write = [value = std::move(retained)](FormatWriterRef& writer, const FormatSpec& spec) {
            ngFormatValueTo(writer, *value, spec);
        };
    }
    return result;
}

template<class T> requires (HasCustomFormat<T> || HasAggregateFormatMetadata<std::remove_cv_t<T>>)
TypedFormatArgument ngFormatValueReferenceArgument(T& value, std::shared_ptr<void> owner) {
    FormatObject result;
    result.sourceTypeName = detail::retainFormatMessage(FormatSourceTypeName<T>::get());
    result.acceptsFormatSpec = true;
    detail::allocateAggregateText([&] {
        result.format = [&value, owner](const FormatSpec& spec) {
            std::string text;
            TypedFormatWriter writer(text);
            ngFormatValueTo(writer, value, spec);
            return detail::retainFormatMessage(std::move(text));
        };
        if constexpr (HasCustomFormatFor<T, FormatWriterRef> ||
                HasAggregateFormatMetadata<std::remove_cv_t<T>>) {
            result.write = [&value, owner = std::move(owner)](FormatWriterRef& writer, const FormatSpec& spec) {
                ngFormatValueTo(writer, value, spec);
            };
        }
    });
    return result;
}

} // namespace nijigenerate::core::translation

#include "typed_format_associative.hpp"
