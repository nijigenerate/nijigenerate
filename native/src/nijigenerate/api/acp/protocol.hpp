#pragma once

#include "protocol_values.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <cstddef>
#include <source_location>
#include <utility>
#include <memory>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::api::acp::protocol {

namespace detail {
// Base-from-member ordering keeps these real fields initialized before the
// canonical Exception constructor, as the source constructor requires.
struct ACPErrorFields {
private:
    struct Fields {
        std::int32_t code = 0;
        compat::RetainedString details;
        Fields(std::int32_t value, compat::RetainedString text)
            : code(value), details(std::move(text)) {}
    };
    std::shared_ptr<Fields> fields_;
public:
    std::int32_t& code;
    compat::RetainedString& details;
    ACPErrorFields(std::int32_t value, compat::RetainedString text)
        : fields_(std::allocate_shared<Fields>(
              nicxlive::core::d_runtime::SourceAllocator<Fields>{}, value, std::move(text))),
          code(fields_->code), details(fields_->details) {}
    ACPErrorFields(const ACPErrorFields& other) noexcept
        : fields_(other.fields_), code(fields_->code), details(fields_->details) {}
    ACPErrorFields(ACPErrorFields&& other) noexcept
        : ACPErrorFields(static_cast<const ACPErrorFields&>(other)) {}
    ACPErrorFields& operator=(const ACPErrorFields&) = delete;
    ACPErrorFields& operator=(ACPErrorFields&&) = delete;
};
}

class ACPError : private detail::ACPErrorFields, public compat::ThrowableException {
public:
    using detail::ACPErrorFields::code;
    using detail::ACPErrorFields::details;

    ACPError(std::int32_t code, compat::RetainedString message,
             compat::RetainedString details = {},
             compat::RetainedString file = std::source_location::current().file_name(),
             std::size_t line = std::source_location::current().line());
    ACPError(ErrorCode code, compat::RetainedString message,
             compat::RetainedString details = {},
             compat::RetainedString file = std::source_location::current().file_name(),
             std::size_t line = std::source_location::current().line());

    virtual core::settings::SettingsJsonValue toJSON() const;
    std::string_view ngExceptionMessage() const noexcept override { return msg.view(); }
    std::string_view ngThrowableTypeName() const noexcept override {
        return "nijigenerate.api.acp.protocol.ACPError";
    }
};

} // namespace nijigenerate::api::acp::protocol

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::protocol::ACPError> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.acp.protocol.ACPError)";
    inline static constexpr char unqualifiedArgument[] = "(ACPError)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.api.acp.protocol.ACPError";
    inline static constexpr char unqualifiedElement[] = "ACPError";
};
template<> struct SourceObjectTypeNames<::nijigenerate::api::acp::protocol::ACPError> {
    inline static constexpr char className[] = "nijigenerate.api.acp.protocol.ACPError";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.api.acp.protocol.ACPError)";
};
}
