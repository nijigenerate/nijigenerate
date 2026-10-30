#pragma once

#include "contracts.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include "detail/json_string.hpp"
#include "detail/json_double.hpp"
#include "detail/sha256.hpp"
#include <algorithm>
#include <charconv>
#include <span>
#include <string>
#include <array>
#include <string_view>
#include <vector>

namespace nijigenerate::autorig::deterministic::contracts {

inline std::uint64_t ngRigUnsigned(core::settings::SettingsJsonValue value) {
    if (value.is_uint64()) return value.as_uint64();
    if (!(value.is_int64() && value.as_int64() >= 0))
        throw compat::ThrowableException(
            detail::copyContractMessage("Expected an unsigned rig integer"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 21);
    return static_cast<std::uint64_t>(value.as_int64());
}

inline double ngRigNumber(core::settings::SettingsJsonValue value) {
    if (!(value.is_double() || value.is_int64() || value.is_uint64()))
        throw compat::ThrowableException{
            detail::copyContractMessage("Expected a numeric rig value"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 26};
    const double result = value.is_double() ? value.as_double() : value.is_int64() ?
        static_cast<double>(value.as_int64()) : static_cast<double>(value.as_uint64());
    if (!std::isfinite(result)) throw compat::ThrowableException{
        detail::copyContractMessage("Nonfinite rig number"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 30};
    return result;
}

inline compat::Slice<double> ngRigNumbers(core::settings::SettingsJsonValue value) {
    compat::Slice<double> result;
    for (const auto entry : value.sourceArrayValue()) result.append(ngRigNumber(entry));
    return result;
}

inline Point2 ngRigPoint(core::settings::SettingsJsonValue value) {
    auto numbers = ngRigNumbers(value);
    if (numbers.size() != 2) throw compat::ThrowableException{
        detail::copyContractMessage("Expected a two-dimensional rig point"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 42};
    return Point2(numbers[0], numbers[1]);
}

inline compat::Slice<Point2> ngRigPoints(core::settings::SettingsJsonValue value) {
    compat::Slice<Point2> result;
    for (const auto entry : value.sourceArrayValue()) result.append(ngRigPoint(entry));
    return result;
}

inline core::settings::SettingsJsonValue ngRigPointsJson(compat::Slice<Point2> points) {
    core::settings::SettingsJsonArray result;
    for (const auto point : points) {
        core::settings::SettingsJsonArray entries{
            core::settings::SettingsJsonValue(point[0]), core::settings::SettingsJsonValue(point[1])};
        result.append(core::settings::SettingsJsonValue(entries));
    }
    return core::settings::SettingsJsonValue(result);
}

inline core::settings::SettingsJsonValue ngRigGet(core::settings::SettingsJsonValue object,
    compat::RetainedStringSlice key, core::settings::SettingsJsonValue fallback = {}) {
    if (!object.is_object()) throw compat::ThrowableException(
        detail::copyContractMessage("Expected a rig object"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 59);
    auto& members = object.sourceObjectRef();
    const auto* value = members.if_contains(key.view());
    return value == nullptr ? fallback : *value;
}

inline double ngRigScalar(core::settings::SettingsJsonValue object,
    compat::RetainedStringSlice key, double fallback) {
    return ngRigNumber(ngRigGet(object, key, core::settings::SettingsJsonValue(fallback)));
}

inline compat::RetainedStringSlice ngRigString(core::settings::SettingsJsonValue object,
    compat::RetainedStringSlice key,
    compat::RetainedStringSlice fallback = compat::RetainedString::emptyLiteral()) {
    return ngRigGet(object, key, core::settings::SettingsJsonValue(core::settings::SettingsString(fallback))).as_string();
}

inline compat::RetainedStringSlice ngRigCanonical(core::settings::SettingsJsonValue value) {
    std::string output;
    if (value.is_object()) {
        const auto& object = value.as_object();
        if (object.isOrdered())
            throw core::settings::SettingsJsonError(
                detail::copyContractMessage("JSONValue object is ordered, cannot return by ref"),
                compat::RetainedString::fromStaticLiteral("std/json.d"), 307);
        std::vector<core::settings::SettingsString> keys;
        for (const auto& member : object)
            detail::allocateContractStorage([&] { keys.push_back(member.name); });
        // AA keys are unique; equal-key permutation cannot affect this order.
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.view() < b.view(); });
        detail::allocateContractStorage([&] { output += '{'; });
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i) detail::allocateContractStorage([&] { output += ','; });
            // JSONValue(key).toString() completes before the appender receives it.
            {
                std::string quotedKey;
                detail::allocateContractStorage([&] { detail::appendJsonString(quotedKey, keys[i].view()); });
                detail::allocateContractStorage([&] { output += quotedKey; });
            }
            detail::allocateContractStorage([&] { output += ':'; });
            const auto child = ngRigCanonical(object.at(keys[i].view()));
            detail::allocateContractStorage([&] { output += child.view(); });
        }
        detail::allocateContractStorage([&] { output += '}'; });
    } else if (value.is_array()) {
        detail::allocateContractStorage([&] { output += '['; });
        std::size_t index = 0;
        for (const auto entry : value.as_array()) {
            if (index++) detail::allocateContractStorage([&] { output += ','; });
            const auto child = ngRigCanonical(entry);
            detail::allocateContractStorage([&] { output += child.view(); });
        }
        detail::allocateContractStorage([&] { output += ']'; });
    } else if (value.is_double()) {
        if (!std::isfinite(value.as_double())) throw compat::ThrowableException(
            detail::copyContractMessage("Nonfinite artifact value"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/contracts.d"), 97);
        output = detail::jsonFiniteDouble(value.as_double());
    } else if (value.is_string()) {
        const auto string = value.as_string();
        detail::allocateContractStorage([&] { detail::appendJsonString(output, string.view()); });
    } else if (value.is_int64() || value.is_uint64()) {
        char buffer[32];
        const auto end = value.is_int64()
            ? std::to_chars(buffer, buffer + sizeof(buffer), value.as_int64()).ptr
            : std::to_chars(buffer, buffer + sizeof(buffer), value.as_uint64()).ptr;
        detail::allocateContractStorage([&] { output.assign(buffer, end); });
    } else if (value.is_bool()) {
        const bool boolean = value.as_bool();
        detail::allocateContractStorage([&] { output = boolean ? "true" : "false"; });
    } else detail::allocateContractStorage([&] { output = "null"; });
    return detail::allocateContractStorage([&] { return compat::RetainedString(std::move(output)); });
}

inline compat::RetainedStringSlice ngRigDigest(core::settings::SettingsJsonValue value) {
    const auto canonical = ngRigCanonical(value);
    detail::Sha256 digest;
    digest.put(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(canonical.data()), canonical.size()));
    const auto bytes = digest.finish();
    constexpr char hex[] = "0123456789ABCDEF";
    std::array<char, 64> output{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        output[2 * i] = hex[bytes[i] >> 4];
        output[2 * i + 1] = hex[bytes[i] & 15];
    }
    return detail::copyContractMessage(std::string_view(output.data(), output.size()));
}

} // namespace nijigenerate::autorig::deterministic::contracts
