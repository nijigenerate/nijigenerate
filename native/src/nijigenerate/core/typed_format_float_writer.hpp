#pragma once
// Phobos printFloat/writeAligned source connection. BSL-1.0;
// retained notices: native/port/licenses/phobos-float-format.txt.

#include "typed_format_decimal.hpp"
#include "typed_format_decimal_signed.hpp"
#include "typed_format_hex.hpp"
#include "typed_format_spec.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core::translation::detail {

inline compat::RetainedStringSlice borrowFloatingFormatComponent(const std::string& value) noexcept {
    return compat::RetainedString::fromBorrowedView(std::string_view(value));
}

// Own the real numeric buffers for the same interval as the Source local
// dec_buf/exp_buf. Writer exceptions propagate after their actual prior puts.
template<class Writer, class Parts>
void writeFloatingFormatComponentsTo(Writer& writer, Parts parts, FormatSpec frame,
    FormatPrecisionType precision) {
    if (frame.spec == 's') frame.spec = 'g';
    if (parts.special) {
        frame.flZero = false;
        precision = FormatPrecisionType::None;
    } else {
        // Both real producers supply this only for their finite branch.
        frame.precision = static_cast<int>(*parts.precision);
    }
    const auto prefix = borrowFloatingFormatComponent(parts.prefix);
    const auto grouped = borrowFloatingFormatComponent(parts.integral);
    const auto fracts = borrowFloatingFormatComponent(parts.fraction);
    const auto suffix = borrowFloatingFormatComponent(parts.suffix);
    ngWriteAlignedFormatTo(writer, prefix, grouped, fracts, suffix, frame, precision);
}

template<class Writer>
void writeFloatingFormatPartsTo(Writer& writer, DecimalFormatParts parts, FormatSpec frame) {
    const auto precision = parts.general ? FormatPrecisionType::AllDigits
        : FormatPrecisionType::FractionalDigits;
    writeFloatingFormatComponentsTo(writer, std::move(parts), std::move(frame), precision);
}

template<class Writer>
void writeFloatingFormatPartsTo(Writer& writer, HexFormatParts parts, FormatSpec frame) {
    writeFloatingFormatComponentsTo(writer, std::move(parts), std::move(frame),
        FormatPrecisionType::FractionalDigits);
}

} // namespace nijigenerate::core::translation::detail
