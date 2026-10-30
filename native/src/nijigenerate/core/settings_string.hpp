#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <serialization_fields.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core::settings {
// Keep the settings string construction surface over the canonical retained
// slice descriptor, including interior nonnull empty strings.
class SettingsString : public compat::RetainedStringSlice {
    using Descriptor = compat::RetainedStringSlice;
    static compat::RetainedString retain(std::string_view value) {
        try {
            return compat::RetainedString(value);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    static compat::RetainedString retain(std::string value) {
        try {
            return compat::RetainedString(std::move(value));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
public:
    SettingsString() noexcept = default;
    SettingsString(std::nullptr_t) noexcept {}
    SettingsString(compat::RetainedString owner) noexcept : Descriptor(std::move(owner)) {}
    SettingsString(Descriptor value) noexcept : Descriptor(std::move(value)) {}
    SettingsString(const char* value) : Descriptor(retain(value ? std::string_view(value) : std::string_view{})) {}
    SettingsString(std::string_view value) : Descriptor(retain(value)) {}
    SettingsString(std::string value) : Descriptor(retain(std::move(value))) {}
    SettingsString(const nicxlive::core::common::NullableString& value) noexcept
        : Descriptor(compat::RetainedString(value)) {}

    static SettingsString emptyLiteral() noexcept {
        return compat::RetainedString::emptyLiteral();
    }
    SettingsString slice(std::size_t first, std::size_t last) const {
        return Descriptor::slice(first, last);
    }
};
} // namespace nijigenerate::core::settings

namespace nicxlive::core::serde {

template<> struct SourceJsonString<::nijigenerate::core::settings::SettingsString> {
    static d_runtime::RetainedString read(const ::nijigenerate::core::settings::SettingsString& value) {
        return value.retainedString();
    }
};

} // namespace nicxlive::core::serde

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::settings::SettingsString> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto name = RetainedString::fromStaticLiteral("string");
        return {name, name, name, name};
    }
};
} // namespace nicxlive::core::d_runtime
