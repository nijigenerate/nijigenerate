#pragma once
#include "contracts.hpp"
#include <nijigenerate/compat/shared_delegate.hpp>
#include <cstddef>
#include <memory>
#include <common/resource_key_map.hpp>
#include <cstdint>
#include <functional>
#include <utility>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <tuple>

namespace nijigenerate::autorig::deterministic::presentation {
using compat::RetainedStringSlice;
using core::settings::SettingsJsonValue;
struct RigReviewTable {
    RetainedStringSlice title;
    compat::Slice<RetainedStringSlice> columns;
    compat::Slice<compat::Slice<RetainedStringSlice>> rows;
    std::size_t page = 0;
};
using RigReviewTranslate = compat::SharedDelegate<RetainedStringSlice(RetainedStringSlice)>;
struct RigReviewMaterialIdHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(value) > sizeof(std::size_t) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
// This is the real AA storage, with the existing native element-copy operation
// lowering the panel's explicit Source .dup. Shared handle copies still alias.
class RigReviewMaterialNameTable : public nicxlive::core::common::ResourceKeyMap<
    std::uint64_t, RetainedStringSlice, RigReviewMaterialIdHash, std::equal_to<std::uint64_t>> {
public:
    RigReviewMaterialNameTable() = default;
    RigReviewMaterialNameTable(const RigReviewMaterialNameTable& other) {
        for (const auto [id, value] : other) (*this)[id] = value;
    }
    RigReviewMaterialNameTable(RigReviewMaterialNameTable&&) = default;
    RigReviewMaterialNameTable& operator=(const RigReviewMaterialNameTable&) = default;
    RigReviewMaterialNameTable& operator=(RigReviewMaterialNameTable&&) = default;
};
// Null handle copies allocate independently; nonnull handles share name updates.
using RigReviewMaterialNames = std::shared_ptr<RigReviewMaterialNameTable>;
RigReviewMaterialNames ngRigReviewMaterialNames(SettingsJsonValue report);
compat::Slice<RigReviewTable> ngRigReviewTables(SettingsJsonValue report,
    RigReviewMaterialNames names = {}, RigReviewTranslate translate = {});
RetainedStringSlice ngRigReviewFieldLabel(RetainedStringSlice field, RigReviewTranslate translate = {});
RetainedStringSlice ngRigReviewClassLabel(RetainedStringSlice value, RigReviewTranslate translate = {});
RetainedStringSlice ngRigReviewEvidenceLabel(RetainedStringSlice value, RigReviewTranslate translate = {});
} // namespace nijigenerate::autorig::deterministic::presentation

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::autorig::deterministic::presentation::RigReviewTable> {
    using Value = ::nijigenerate::autorig::deterministic::presentation::RigReviewTable;
    inline static constexpr std::string_view typeName = "RigReviewTable";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("title", offsetof(Value, title), &Value::title),
            ngFormatAggregateMember("columns", offsetof(Value, columns), &Value::columns),
            ngFormatAggregateMember("rows", offsetof(Value, rows), &Value::rows),
            ngFormatAggregateMember("page", offsetof(Value, page), &Value::page)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::presentation::RigReviewTable> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.presentation.RigReviewTable)";
    inline static constexpr char unqualifiedArgument[] = "(RigReviewTable)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.presentation.RigReviewTable";
    inline static constexpr char unqualifiedElement[] = "RigReviewTable";
};
}
