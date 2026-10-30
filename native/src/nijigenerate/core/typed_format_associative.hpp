#pragma once
// Associative-array formatting from Phobos std/format/internal/write.d.
// Copyright The D Language Foundation 2000-2013. BSL-1.0;
// retained license: native/port/licenses/phobos-float-format.txt.
#include "typed_format.hpp"
#include <common/resource_key_map.hpp>
#include <common/string_key_map.hpp>

namespace nicxlive::core::common {
template<class K, class V, class Hash, class Equal>
::nijigenerate::core::translation::TypedFormatArgument ngFormatArgument(ResourceKeyMap<K, V, Hash, Equal> value);
template<class Value>
::nijigenerate::core::translation::TypedFormatArgument ngFormatArgument(StringKeyMap<Value> value);
}

namespace nijigenerate::core::translation::detail {

inline void enforceAssociativeFormat(bool condition, std::string_view message, std::size_t line) {
    if (!condition) formatFailure(copyFormatTypeName(message), "std/format/internal/write.d", line);
}

template<class Writer, class Value>
void writeAssociativeElement(Writer& writer, Value& value, const FormatSpec& spec, bool unquoted) {
    using T = std::remove_cvref_t<Value>;
    if (unquoted || HasCustomFormatFor<Value, Writer> || HasFormatObjectTraits<T> || HasAggregateFormatMetadata<T>) {
        ngFormatValueTo(writer, value, spec);
    } else {
        const auto argument = ngFormatArgument(value);
        if (writeTypedElementValueTo(writer, argument, spec)) return;
        auto text = formatTypedElement(argument, ngParsedFormatSpec(spec));
        ngTypedFormatPut(writer, compat::RetainedStringSlice(retainFormatMessage(std::move(text))));
    }
}

template<class Writer, class Map>
void formatAssociativeValueTo(Writer& writer, Map value, const FormatSpec& spec) {
    const auto conversion = spec.spec;
    if (conversion != 's' && conversion != '(') {
        auto message = allocateAggregateText([&] {
            auto text = std::string("incompatible format character for associative array argument: %");
            text += conversion;
            return text;
        });
        formatFailure(std::move(message), "std/format/internal/write.d", 1657);
    }
    const auto pattern = conversion == '(' ? spec.nested :
        compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("%s:%s, "));
    bool keyFirst = true;
    auto noop = [](std::string_view) {};
    FormatSpec test(pattern);
    enforceAssociativeFormat(test.writeUpToNextSpec(noop),
        "nested format string for associative array contains no format specifier", 1669);
    enforceAssociativeFormat(test.indexStart <= 2,
        "positional parameter in nested format string for associative array may only be 1 or 2", 1671);
    if (test.indexStart == 2) keyFirst = false;
    enforceAssociativeFormat(test.writeUpToNextSpec(noop),
        "nested format string for associative array contains only one format specifier", 1676);
    enforceAssociativeFormat(test.indexStart <= 2,
        "positional parameter in nested format string for associative array may only be 1 or 2", 1678);
    enforceAssociativeFormat(test.indexStart == 0 || ((test.indexStart == 2) == keyFirst),
        "wrong combination of positional parameters in nested format string", 1680);
    enforceAssociativeFormat(!test.writeUpToNextSpec(noop),
        "nested format string for associative array contains more than two format specifiers", 1683);

    std::size_t index = 0;
    const auto end = value.size();
    if (conversion == 's') ngTypedFormatPut(writer, compat::RetainedStringSlice(FormatSpec::seqBefore));
    const auto writeEntry = [&](const auto& storedKey, auto& mapped) {
        auto key = storedKey;
        FormatSpec current(pattern);
        for (int position = 1; position != 3; ++position) {
            current.writeUpToNextSpec(writer);
            if (keyFirst == (position == 1)) writeAssociativeElement(writer, key, current, spec.flDash);
            else writeAssociativeElement(writer, mapped, current, spec.flDash);
        }
        if (!spec.sep.isNull()) {
            current.writeUpToNextSpec(writer);
            if (++index != end) ngTypedFormatPut(writer, spec.sep);
        } else if (++index != end) current.writeUpToNextSpec(writer);
    };
    if constexpr (requires { value.forEachEntry(writeEntry); }) value.forEachEntry(writeEntry);
    else {
        for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
            auto& entry = *iterator;
            writeEntry(entry.first, entry.second);
        }
    }
    if (conversion == 's') ngTypedFormatPut(writer, compat::RetainedStringSlice(FormatSpec::seqAfter));
}

template<class Map> TypedFormatArgument makeAssociativeArgument(Map value) {
    FormatObject result;
    result.acceptsFormatSpec = true;
    result.format = [value](const FormatSpec& spec) mutable {
        std::string text;
        TypedFormatWriter writer(text);
        formatAssociativeValueTo(writer, value, spec);
        return retainFormatMessage(std::move(text));
    };
    result.write = [value = std::move(value)](FormatWriterRef& writer, const FormatSpec& spec) mutable {
        formatAssociativeValueTo(writer, value, spec);
    };
    return result;
}

} // namespace nijigenerate::core::translation::detail

namespace nicxlive::core::common {

template<class K, class V, class Hash, class Equal>
::nijigenerate::core::translation::TypedFormatArgument ngFormatArgument(ResourceKeyMap<K, V, Hash, Equal> value) {
    return ::nijigenerate::core::translation::detail::makeAssociativeArgument(std::move(value));
}

template<class Value>
::nijigenerate::core::translation::TypedFormatArgument ngFormatArgument(StringKeyMap<Value> value) {
    return ::nijigenerate::core::translation::detail::makeAssociativeArgument(std::move(value));
}

} // namespace nicxlive::core::common
