#pragma once
#include <compat/object_metadata.hpp>

namespace nijigenerate::core::settings { class SettingsJsonValue; }

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::settings::SettingsJsonValue> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(std.json.JSONValue)"),
            RetainedString::fromStaticLiteral("(JSONValue)"),
            RetainedString::fromStaticLiteral("std.json.JSONValue"),
            RetainedString::fromStaticLiteral("JSONValue")};
    }
};
} // namespace nicxlive::core::d_runtime
