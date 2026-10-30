#include "detail/bone_json_apply.hpp"
// Integer conversion adaptation: Phobos std.conv, Copyright The D Language
// Foundation 2007-. BSL-1.0, see port/licenses/phobos-rig-json.txt.
#include "detail/bone_strict_scope.hpp"
#include "detail/phobos_float_parse.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {
namespace {
using core::settings::SettingsJsonValue;

SettingsJsonValue parseApplyJson(compat::RetainedString text) {
    autorig::json::detail::FirstPassReader reader{compat::RetainedStringSlice(text)};
    SettingsJsonValue result;
    // Default std.json.parseJSON calls parseValue, including for empty input;
    // trailing non-whitespace is rejected only by its separate strict option.
    reader.parseValue(result);
    return result;
}

using DepthBoneConvOverflow = compat::PhobosConvOverflowException;

[[noreturn]] void failUnsignedKey(std::string_view remaining, std::size_t sourceLine) try {
    if (remaining.empty()) throw phobos_float_detail::FloatConvException(
        compat::RetainedString::fromStaticLiteral(
            "Unexpected end of input when converting from type string to type ulong"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), sourceLine);
    const auto length = phobos_float_detail::trailingScalarLength(remaining);
    std::string message = "Unexpected '";
    if (remaining.front() == '\n') message += "\\n";
    else message.append(remaining.data(), length);
    message += "' when converting from type string to type ulong";
    throw phobos_float_detail::FloatConvException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), sourceLine);
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
}

// Current Phobos std.conv.parse!ulong followed by toImpl's full-consumption
// check. Unsigned parsing does not consume either sign or whitespace.
std::uint64_t unsignedKey(std::string_view source) try {
    std::size_t cursor = 0;
    std::uint64_t value = 0;
    while (cursor < source.size()) {
        const auto byte = static_cast<unsigned char>(source[cursor]);
        if (byte < '0' || byte > '9') break;
        const auto digit = static_cast<unsigned>(byte - '0');
        if (value > std::numeric_limits<std::uint64_t>::max() / 10 ||
            (value == std::numeric_limits<std::uint64_t>::max() / 10 && digit > 5))
            throw DepthBoneConvOverflow(
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527);
        value = value * 10 + digit;
        ++cursor;
    }
    if (!cursor || cursor != source.size()) failUnsignedKey(source.substr(cursor), cursor ? 2013 : 2547);
    return value;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
}
} // namespace

void applyRuleJson(ext::nodes::ExDepthInfluenceRule& rule, compat::RetainedString text) {
    auto json = parseApplyJson(text);
    auto object = json.sourceObjectRef();
    if (auto value = object.if_contains("maxInfluences"))
        rule.maxInfluences = static_cast<std::uint32_t>(value->as_int64());
    if (auto value = object.if_contains("radiusScale"))
        rule.radiusScale = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("radiusScale"));
    if (auto value = object.if_contains("minimumRadius"))
        rule.minimumRadius = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("minimumRadius"));
    if (auto value = object.if_contains("falloff")) rule.falloff = value->as_string().retainedString();
    if (auto value = object.if_contains("multipliersByBoneUuid")) {
        rule.multipliersByBoneUuid.clear();
        auto multipliers = value->sourceObjectRef();
        for (auto member : multipliers) {
            constexpr std::string_view prefix = "multipliersByBoneUuid.";
            const auto key = member.key();
            // LDC 1.41 reorderSettingAAElem extracts the AA base and key
            // before the RHS, then accesses the destination after the RHS.
            // A malformed key therefore fails before label/value conversion.
            auto& destination = rule.multipliersByBoneUuid;
            const auto uuid = unsignedKey(key);
            compat::RetainedString name;
            try {
                std::string label;
                label.reserve(prefix.size() + key.size());
                label.append(prefix);
                if (!key.empty()) label.append(key);
                name = compat::RetainedString(std::move(label));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            }
            const auto number = strictJsonNumber(member.value(), name);
            destination[uuid] = number;
        }
    }
    if (rule.maxInfluences == 0) rule.maxInfluences = 1;
}

void applySourceSettingsJson(ext::nodes::ExDepthBoneSourceSettings& setting, compat::RetainedString text) {
    auto json = parseApplyJson(text);
    auto object = json.sourceObjectRef();
    if (auto value = object.if_contains("weight"))
        setting.weight = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("weight"));
    if (auto value = object.if_contains("depthOffset"))
        setting.depthOffset = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("depthOffset"));
    if (auto value = object.if_contains("depthScale"))
        setting.depthScale = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("depthScale"));
    if (auto value = object.if_contains("rotation"))
        setting.rotation = strictJsonNumber(*value, compat::RetainedString::fromStaticLiteral("rotation"));
    if (!std::isfinite(setting.weight)) setting.weight = 1.0f;
    if (!std::isfinite(setting.depthOffset)) setting.depthOffset = 0.0f;
    if (!std::isfinite(setting.depthScale)) setting.depthScale = 1.0f;
    setting.rotation = ext::nodes::ngNormalizeDepthBoneSourceRotation(setting.rotation);
    if (setting.weight < 0) setting.weight = 0;
    if (setting.depthScale < 0.01f) setting.depthScale = 0.01f;
}

} // namespace nijigenerate::commands::depth::bone::detail
