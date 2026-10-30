#pragma once

#include <cstdint>
#include <cstddef>
#include <compare>
#include <functional>
#include <concepts>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

namespace nijigenerate::api::acp::protocol {
inline constexpr std::int32_t ACP_PROTOCOL_VERSION = 1;
inline const auto JSONRPC_VERSION = compat::RetainedString::fromStaticLiteral("2.0");

// A scoped C++ enum value-initializes to zero, whereas this source enum starts
// at parseError. Keep its nominal signed32 value and full unnamed-value domain.
class ErrorCode {
    std::int32_t value_ = -32700;
public:
    constexpr ErrorCode() noexcept = default;
    constexpr explicit ErrorCode(std::int32_t value) noexcept : value_(value) {}
    constexpr explicit operator std::int32_t() const noexcept { return value_; }
    template<std::integral Integer> constexpr explicit operator Integer() const noexcept {
        return static_cast<Integer>(value_);
    }
    template<std::floating_point Float> constexpr explicit operator Float() const noexcept {
        return static_cast<Float>(value_);
    }
    friend constexpr auto operator<=>(ErrorCode, ErrorCode) = default;

    static const ErrorCode parseError;
    static const ErrorCode invalidRequest;
    static const ErrorCode methodNotFound;
    static const ErrorCode invalidParams;
    static const ErrorCode internalError;
};
inline constexpr ErrorCode ErrorCode::parseError{-32700};
inline constexpr ErrorCode ErrorCode::invalidRequest{-32600};
inline constexpr ErrorCode ErrorCode::methodNotFound{-32601};
inline constexpr ErrorCode ErrorCode::invalidParams{-32602};
inline constexpr ErrorCode ErrorCode::internalError{-32603};

inline const auto ACP_METHOD_INITIALIZE = compat::RetainedString::fromStaticLiteral("initialize");
inline const auto ACP_METHOD_PING = compat::RetainedString::fromStaticLiteral("ping");
inline const auto ACP_METHOD_SESSION_NEW = compat::RetainedString::fromStaticLiteral("session/new");
inline const auto ACP_METHOD_SESSION_PROMPT = compat::RetainedString::fromStaticLiteral("session/prompt");
inline const auto ACP_METHOD_SESSION_CANCEL = compat::RetainedString::fromStaticLiteral("session/cancel");
inline const auto ACP_METHOD_SESSION_UPDATE = compat::RetainedString::fromStaticLiteral("session/update");
inline const auto ACP_METHOD_APPLY_EDIT = compat::RetainedString::fromStaticLiteral("workspace/applyEdit");
inline const auto ACP_METHOD_STATUS = compat::RetainedString::fromStaticLiteral("notifications/status");
inline const auto ACP_METHOD_USER_MSG = compat::RetainedString::fromStaticLiteral("editor/userMessage");
} // namespace nijigenerate::api::acp::protocol

namespace nijigenerate::core::translation {
template<> struct FormatNumericEnumTraits<::nijigenerate::api::acp::protocol::ErrorCode> {
    using Value = ::nijigenerate::api::acp::protocol::ErrorCode;
    using Underlying = std::int32_t;
    inline static constexpr std::string_view name = "ErrorCode";
    static constexpr Underlying underlying(Value value) { return static_cast<Underlying>(value); }
    static constexpr Value fromUnderlying(Underlying value) { return Value(value); }
    inline static constexpr std::array members{
        FormatNumericEnumMember<Value>{Value::parseError, "parseError"},
        FormatNumericEnumMember<Value>{Value::invalidRequest, "invalidRequest"},
        FormatNumericEnumMember<Value>{Value::methodNotFound, "methodNotFound"},
        FormatNumericEnumMember<Value>{Value::invalidParams, "invalidParams"},
        FormatNumericEnumMember<Value>{Value::internalError, "internalError"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::protocol::ErrorCode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.acp.protocol.ErrorCode)";
    inline static constexpr char unqualifiedArgument[] = "(ErrorCode)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.api.acp.protocol.ErrorCode";
    inline static constexpr char unqualifiedElement[] = "ErrorCode";
};
} // namespace nicxlive::core::d_runtime

template<> struct std::hash<nijigenerate::api::acp::protocol::ErrorCode> {
    std::size_t operator()(nijigenerate::api::acp::protocol::ErrorCode value) const noexcept {
        return std::hash<std::int32_t>{}(static_cast<std::int32_t>(value));
    }
};
