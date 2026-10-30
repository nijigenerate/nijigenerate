#pragma once

#include <common/nullable_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::ext::nodes {

// Preserve the actual immutable bytes and null/empty distinction at the SDK
// boundary. The retained descriptor owns its backing even for an empty subview.
inline nicxlive::core::common::NullableString ngRetainNodeName(compat::RetainedStringSlice text) {
    return text.retainedString().toNullableString();
}

inline compat::RetainedStringSlice ngRetainNodeName(const nicxlive::core::common::NullableString& text) {
    return compat::RetainedStringSlice(compat::RetainedString(text));
}

} // namespace nijigenerate::ext::nodes
