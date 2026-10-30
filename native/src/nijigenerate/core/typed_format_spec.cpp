// Copyright The D Language Foundation 2000-2013. BSL-1.0;
// retained license: native/port/licenses/phobos-float-format.txt.
#include "typed_format.hpp"

namespace nijigenerate::core::translation {
FormatSpec ngSingleFormatSpec(compat::RetainedStringSlice text) {
    if (text.size() < 2)
        detail::formatFailure("fmt must be at least 2 characters long", "std/format/spec.d", 890);
    if (text.view().front() != '%')
        detail::formatFailure("fmt must start with a '%' character", "std/format/spec.d", 891);
    if (text.view()[1] == '%')
        detail::formatFailure("'%%' is not a permissible format specifier", "std/format/spec.d", 892);
    FormatSpec result(text);
    auto ignore = [](compat::RetainedStringSlice) {};
    result.writeUpToNextSpec(ignore);
    if (!result.trailing.empty()) detail::formatFailure(
        detail::allocateAggregateText([&] {
            return "Trailing characters in fmt string: '" + std::string(result.trailing.view());
        }), "std/format/spec.d", 903);
    return result;
}
detail::ParsedTypedFormat ngParsedFormatSpec(const FormatSpec& source) {
    detail::ParsedTypedFormat result;
    result.conversion = source.spec;
    result.left = source.flDash; result.zero = source.flZero;
    result.space = source.flSpace; result.plus = source.flPlus;
    result.alternate = source.flHash; result.center = source.flEqual;
    result.grouping = source.flSeparator;
    result.width = source.width > 0 ? static_cast<std::size_t>(source.width) : 0;
    result.dynamicWidth = {source.width == FormatSpec::DYNAMIC, {}};
    if (source.precision != FormatSpec::UNSPECIFIED)
        result.precision = static_cast<std::size_t>(source.precision);
    result.dynamicPrecision = {source.precision == FormatSpec::DYNAMIC, {}};
    result.groupSize = source.separators;
    result.dynamicGroup = source.separators == FormatSpec::DYNAMIC;
    result.separator = source.separatorChar;
    result.dynamicSeparator = source.dynamicSeparatorChar;
    if (source.indexStart) result.argument = source.indexStart - 1;
    if (source.indexEnd) result.lastArgument = source.indexEnd - 1;
    result.consumeThrough = source.indexEnd;
    result.remainingArguments = source.indexEnd == UINT16_MAX;
    result.nested = detail::copyFormatTypeName(source.nested.view());
    if (!source.sep.isNull()) result.nestedSeparator = detail::copyFormatTypeName(source.sep.view());
    return result;
}
FormatSpec ngPublicFormatSpec(const detail::ParsedTypedFormat& source) {
    FormatSpec result;
    result.spec = source.conversion;
    result.width = static_cast<int>(source.width);
    result.precision = source.precision ? static_cast<int>(*source.precision) : FormatSpec::UNSPECIFIED;
    result.flDash = source.left; result.flZero = source.zero;
    result.flSpace = source.space; result.flPlus = source.plus;
    result.flHash = source.alternate; result.flEqual = source.center; result.flSeparator = source.grouping;
    if (source.grouping) result.separators = source.groupSize;
    result.separatorChar = source.separator;
    result.dynamicSeparatorChar = source.dynamicSeparator;
    if (source.argument) result.indexStart = static_cast<std::uint16_t>(*source.argument + 1);
    if (source.lastArgument) result.indexEnd = static_cast<std::uint16_t>(*source.lastArgument + 1);
    if (source.remainingArguments) result.indexEnd = UINT16_MAX;
    if (source.conversion == '(') result.nested = compat::RetainedString(source.nested);
    if (source.nestedSeparator) result.sep = compat::RetainedString(*source.nestedSeparator);
    return result;
}
void FormatSpec::checkTerminated() const {
    if (trailing.size() < 2)
        detail::formatFailure("Unterminated format specifier: \"%\"", "std/format/spec.d", 262);
}
void FormatSpec::fillUp() {
    std::size_t offset = 0;
    // The existing canonical parser mutates this same public state at the
    // original expressions, including partial state on a failed parse.
    (void)detail::parseTypedFormat(trailing.view(), offset, this);
}
compat::RetainedString FormatSpec::getCurFmtStr() const {
    std::string text;
    TypedFormatWriter output(text);
    output.put('%');
    auto writer = [&](compat::RetainedStringSlice part) { output.put(part); };
    const auto append = [&](auto value) {
        ngFormatDefaultValueTo(writer, value);
    };
    if (indexStart != 0) { append(indexStart); output.put('$'); }
    if (flDash) output.put('-');
    if (flZero) output.put('0');
    if (flSpace) output.put(' ');
    if (flPlus) output.put('+');
    if (flEqual) output.put('=');
    if (flHash) output.put('#');
    if (width != 0) append(width);
    if (precision != UNSPECIFIED) { output.put('.'); append(precision); }
    if (flSeparator) output.put(',');
    if (separators != UNSPECIFIED) append(separators);
    output.put(spec);
    return compat::RetainedString(std::move(text));
}
compat::RetainedString FormatSpec::toString() const {
    std::string text;
    detail::allocateAggregateText([&] { text.reserve(200 + trailing.size()); });
    TypedFormatWriter output(text);
    writeDescription([&](compat::RetainedStringSlice part) { output.put(part); },
        [&](char value) { output.put(value); });
    return compat::RetainedString(std::move(text));
}
} // namespace nijigenerate::core::translation
