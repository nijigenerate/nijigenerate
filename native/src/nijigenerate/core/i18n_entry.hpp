#pragma once

#include "nijigenerate/compat/retained_string.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <new>
#include <source_location>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace nijigenerate::core::i18n {
struct TLEntry;
namespace detail { void refreshLocaleNameCString(TLEntry& entry); }

struct TLEntry {
    compat::RetainedString humanName;
    const char* humanNameC{};
    compat::RetainedString code;
    compat::RetainedString file;
    compat::RetainedString path;

    TLEntry() = default;
    TLEntry(compat::RetainedString name, const char* nameC, compat::RetainedString languageCode,
        compat::RetainedString filename, compat::RetainedString directory)
        : humanName(std::move(name)), humanNameC(nameC), code(std::move(languageCode)),
          file(std::move(filename)), path(std::move(directory)) {}
private:
    // Generated backing is retained separately from the independently mutable
    // humanName descriptor. An externally supplied humanNameC is never freed.
    compat::RetainedString generatedNameBacking_;
    friend void detail::refreshLocaleNameCString(TLEntry& entry);
};

namespace detail {
template<class Operation>
decltype(auto) allocateLocaleStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline void refreshLocaleNameCString(TLEntry& entry) {
    const auto input = entry.humanName;
    const compat::SourceCString output{compat::RetainedStringSlice(input)};
    compat::detail::ngSourceCStringPostconditions(input.view(), output.data());
    entry.generatedNameBacking_ = output.retainedString();
    entry.humanNameC = entry.generatedNameBacking_.data();
}
} // namespace detail
} // namespace nijigenerate::core::i18n

namespace nijigenerate::core::translation {

template<> struct FormatAggregateTraits<::nijigenerate::core::i18n::TLEntry> {
    using Value = ::nijigenerate::core::i18n::TLEntry;
    inline static constexpr std::string_view typeName = "TLEntry";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("humanName", offsetof(Value, humanName), &Value::humanName),
            ngFormatAggregateMember("humanNameC", offsetof(Value, humanNameC), &Value::humanNameC),
            ngFormatAggregateMember("code", offsetof(Value, code), &Value::code),
            ngFormatAggregateMember("file", offsetof(Value, file), &Value::file),
            ngFormatAggregateMember("path", offsetof(Value, path), &Value::path)};
    }
};

} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::i18n::TLEntry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.i18n.TLEntry)";
    inline static constexpr char unqualifiedArgument[] = "(TLEntry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.i18n.TLEntry";
    inline static constexpr char unqualifiedElement[] = "TLEntry";
};
}
