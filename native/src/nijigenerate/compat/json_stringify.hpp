#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/autorig/deterministic/detail/json_string.hpp>
#include <nijigenerate/autorig/deterministic/detail/json_double.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::compat {
namespace json_detail {
// Evaluate real JSON/numeric providers before entering a primitive storage operation.
template<class Operation> auto allocateStringifyStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
inline void append(std::string& output, const core::settings::SettingsJsonValue& value) {
    namespace format = autorig::deterministic::contracts::detail;
    if (value.is_object()) {
        std::vector<const core::settings::SettingsJsonMember*> members;
        for (const auto& member : value.as_object())
            allocateStringifyStorage([&] { members.push_back(&member); });
        if (!value.as_object().isOrdered()) {
            std::sort(members.begin(), members.end(), [](const auto* a, const auto* b) {
                return a->key() < b->key();
            });
        }
        allocateStringifyStorage([&] { output += '{'; });
        bool first = true;
        for (const auto* member : members) {
            if (!first) allocateStringifyStorage([&] { output += ','; });
            first = false;
            const auto key = member->key();
            allocateStringifyStorage([&] { format::appendJsonString(output, key); });
            allocateStringifyStorage([&] { output += ':'; });
            append(output, member->value());
        }
        allocateStringifyStorage([&] { output += '}'; });
    } else if (value.is_array()) {
        allocateStringifyStorage([&] { output += '['; });
        bool first = true;
        for (const auto& element : value.as_array()) {
            if (!first) allocateStringifyStorage([&] { output += ','; });
            first = false;
            append(output, element);
        }
        allocateStringifyStorage([&] { output += ']'; });
    } else if (value.is_string()) {
        const auto string = value.as_string();
        allocateStringifyStorage([&] { format::appendJsonString(output, string.view()); });
    } else if (value.is_double()) {
        const double number = value.as_double();
        if (!std::isfinite(number)) {
            throw core::settings::SettingsJsonError(std::isnan(number)
                ? "Cannot encode NaN. Consider passing the specialFloatLiterals flag."
                : "Cannot encode Infinity. Consider passing the specialFloatLiterals flag.");
        }
        const auto formatted = format::jsonFiniteDouble(number);
        allocateStringifyStorage([&] { output += formatted; });
    } else if (value.is_int64()) {
        const auto number = value.as_int64();
        allocateStringifyStorage([&] { output += std::to_string(number); });
    } else if (value.is_uint64()) {
        const auto number = value.as_uint64();
        allocateStringifyStorage([&] { output += std::to_string(number); });
    } else if (value.is_bool()) {
        const auto boolean = value.as_bool();
        allocateStringifyStorage([&] { output += boolean ? "true" : "false"; });
    } else allocateStringifyStorage([&] { output += "null"; });
}
} // namespace json_detail

// Current Phobos JSONValue.toString with default options, including ordered objects.
inline RetainedString ngJsonStringify(const core::settings::SettingsJsonValue& value) {
    std::string output;
    json_detail::append(output, value);
    return json_detail::allocateStringifyStorage([&] { return RetainedString(std::move(output)); });
}
} // namespace nijigenerate::compat
