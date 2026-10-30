#include "detail/bone_json.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <array>
#include <charconv>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {
namespace {

template<std::size_t N>
core::settings::SettingsString literalKey(const char (&value)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(value);
}

} // namespace

core::settings::SettingsJsonValue ruleToJson(ext::nodes::ExDepthInfluenceRule& rule) {
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    using core::settings::SettingsString;

    SettingsJsonObject object;
    object[literalKey("maxInfluences")] = SettingsJsonValue(rule.maxInfluences);
    object[literalKey("radiusScale")] = SettingsJsonValue(rule.radiusScale);
    object[literalKey("minimumRadius")] = SettingsJsonValue(rule.minimumRadius);
    object[literalKey("falloff")] = SettingsJsonValue(SettingsString(rule.falloff));

    SettingsJsonObject multipliers;
    // Retain the original AA and copy each foreach pair before key allocation.
    const auto sourceMultipliers = rule.multipliersByBoneUuid;
    for (const auto [uuid, value] : sourceMultipliers.entries()) {
        // Every uint64 decimal value fits, including all 20 digits of its max.
        // Never narrow through Node's uint UUID, a signed integer or a float.
        std::array<char, std::numeric_limits<std::uint64_t>::digits10 + 1> digits;
        const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), uuid);
        SettingsString key;
        try {
            key = SettingsString(std::string_view(digits.data(),
                static_cast<std::size_t>(converted.ptr - digits.data())));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
        multipliers[std::move(key)] = SettingsJsonValue(value);
    }
    // A typed null-backed AA still constructs an object, even with no entries.
    object[literalKey("multipliersByBoneUuid")] = SettingsJsonValue(std::move(multipliers));
    return SettingsJsonValue(std::move(object));
}

} // namespace nijigenerate::commands::depth::bone::detail
