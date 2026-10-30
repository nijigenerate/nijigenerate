#pragma once
#include "contracts.hpp"
#include <cstddef>
#include <cstdint>
#include <compare>
#include <concepts>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

namespace nijigenerate::autorig::deterministic::review {
using core::settings::SettingsJsonValue;
using compat::RetainedStringSlice;
// The original nominal ubyte enum initializes to its first member, role.
class RigMaterialField {
    std::uint8_t value_ = 1;
public:
    constexpr RigMaterialField() noexcept = default;
    constexpr explicit RigMaterialField(std::uint8_t value) noexcept : value_(value) {}
    template<std::integral Integer> constexpr explicit operator Integer() const noexcept {
        return static_cast<Integer>(value_);
    }
    template<std::floating_point Float> constexpr explicit operator Float() const noexcept {
        return static_cast<Float>(value_);
    }
    friend constexpr auto operator<=>(RigMaterialField, RigMaterialField) = default;
    static const RigMaterialField role, stationary, feature, side;
};
inline constexpr RigMaterialField RigMaterialField::role{1};
inline constexpr RigMaterialField RigMaterialField::stationary{2};
inline constexpr RigMaterialField RigMaterialField::feature{4};
inline constexpr RigMaterialField RigMaterialField::side{8};
bool ngRigMaterialForcedStatic(bool active, RetainedStringSlice role, RetainedStringSlice semanticSource);
bool ngRigMaterialStaticPreference(SettingsJsonValue material);
SettingsJsonValue ngRigEditedMaterialOverride(SettingsJsonValue previous, std::uint8_t fields,
    RetainedStringSlice role, bool stationary, RetainedStringSlice feature, RetainedStringSlice side);
SettingsJsonValue ngRigReviewCompact(SettingsJsonValue value, std::size_t depth = 0);
SettingsJsonValue ngRigClassificationReview(SettingsJsonValue observation, SettingsJsonValue program);
bool ngRigReviewEnabled(SettingsJsonValue review, RetainedStringSlice id);
RetainedStringSlice ngRigReviewOperationId(RetainedStringSlice kind, SettingsJsonValue operation,
    RetainedStringSlice name = compat::RetainedString::emptyLiteral());
SettingsJsonValue ngRigControlGeometry(SettingsJsonValue controls);
SettingsJsonValue ngRigReviewControls(SettingsJsonValue controls, SettingsJsonValue review,
    SettingsJsonValue& operations);
SettingsJsonValue ngRigReviewChanges(SettingsJsonValue before, SettingsJsonValue after);
} // namespace nijigenerate::autorig::deterministic::review

namespace nijigenerate::core::translation {
template<> struct FormatNumericEnumTraits<::nijigenerate::autorig::deterministic::review::RigMaterialField> {
    using Value = ::nijigenerate::autorig::deterministic::review::RigMaterialField;
    using Underlying = std::uint8_t;
    inline static constexpr std::string_view name = "RigMaterialField";
    static constexpr Underlying underlying(Value value) { return static_cast<Underlying>(value); }
    static constexpr Value fromUnderlying(Underlying value) { return Value(value); }
    inline static constexpr std::array members{
        FormatNumericEnumMember<Value>{Value::role, "role"},
        FormatNumericEnumMember<Value>{Value::stationary, "stationary"},
        FormatNumericEnumMember<Value>{Value::feature, "feature"},
        FormatNumericEnumMember<Value>{Value::side, "side"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::review::RigMaterialField> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral(
                    "(nijigenerate.autorig.deterministic.review.RigMaterialField)"),
            RetainedString::fromStaticLiteral("(RigMaterialField)"),
            RetainedString::fromStaticLiteral("nijigenerate.autorig.deterministic.review.RigMaterialField"),
            RetainedString::fromStaticLiteral("RigMaterialField")};
    }
};
} // namespace nicxlive::core::d_runtime
