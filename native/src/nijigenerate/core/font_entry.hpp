#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cstddef>
#include <tuple>
namespace nijigenerate::core::font {
struct FontEntry {
    compat::RetainedString name;
    compat::RetainedString lang;
    compat::RetainedString file;
};
}

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::core::font::FontEntry> {
    using Value = ::nijigenerate::core::font::FontEntry;
    inline static constexpr std::string_view typeName = "FontEntry";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("name", offsetof(Value, name), &Value::name),
            ngFormatAggregateMember("lang", offsetof(Value, lang), &Value::lang),
            ngFormatAggregateMember("file", offsetof(Value, file), &Value::file)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::font::FontEntry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.font.FontEntry)";
    inline static constexpr char unqualifiedArgument[] = "(FontEntry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.font.FontEntry";
    inline static constexpr char unqualifiedElement[] = "FontEntry";
};
}
