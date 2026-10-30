#pragma once

#include <nijigenerate/compat/phobos_unsigned_conversion.hpp>

namespace nijigenerate::autorig::deterministic::presentation::detail {
using ReviewConversionException = compat::phobos::ConvException;
using ReviewConversionOverflowException = compat::phobos::ConvOverflowException;

inline std::uint64_t ngUnsignedNodeId(std::string_view source) {
    return compat::phobos::ngUnsignedDecimal64(source);
}
} // namespace nijigenerate::autorig::deterministic::presentation::detail
