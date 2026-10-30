#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <serialization_fields.hpp>
#include <compat/assert_error.hpp>

namespace nijigenerate::compat::detail {

// std.conv enum-to-string uses the identifier, and delegates unknown values to
// the normal cast(Name)+base formatter. It does not use serde key annotations.
template<class Enum> RetainedString enumJsonKey(Enum value) {
    using Underlying = std::underlying_type_t<Enum>;
    for (const auto& member : core::translation::FormatEnumTraits<Enum>::members) {
        if (static_cast<Underlying>(member.value) == static_cast<Underlying>(value))
            return RetainedString::fromStaticView(member.name);
    }
    std::string text;
    core::translation::TypedFormatWriter writer(text);
    core::translation::FormatSpec spec;
    core::translation::ngFormatValueTo(writer, value, spec);
    return core::translation::detail::retainFormatMessage(std::move(text));
}

// Used only by exact declaring producers whose original enum members have no
// serde renaming, ignore or proxy UDAs. The table belongs to that declaration.
template<class Enum, class Serializer>
void serializePlainSourceEnum(Enum value, Serializer& serializer) {
    for (const auto& member : core::translation::FormatEnumTraits<Enum>::members) {
        using Underlying = std::underlying_type_t<Enum>;
        if (static_cast<Underlying>(member.value) != static_cast<Underlying>(value)) continue;
        serializer.putValue(RetainedString::fromStaticView(member.name));
        return;
    }
    // mir.serde.serdeGetKeyOut asserts on an unregistered enum value.
    nicxlive::core::d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
}

} // namespace nijigenerate::compat::detail
