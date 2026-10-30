#pragma once

#include "out_of_memory_error.hpp"
#include "retained_string.hpp"
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nijigenerate::compat {

// Actual mir.ndslice.slice.mir_slice_kind declaration (slice.d175-184).
// This type does not select a kind for any native view representation.
enum class SourceMirSliceKind : std::int32_t {
    universal = 0,
    canonical = 1,
    contiguous = 2,
};

// All tokens belong to one selected Source printer context. In particular,
// dimension and kind expressions are not reconstructed from native integers.
struct SourceSliceTypeContext {
    RetainedString iteratorArgument;
    RetainedString dimensionArgument;
    RetainedString kindArgument;
    std::vector<RetainedString> labelArguments;
    RetainedString qualifierPrefix;
    RetainedString qualifierSuffix;

    SourceSliceTypeContext(RetainedString iteratorArgument, RetainedString dimensionArgument,
        RetainedString kindArgument, std::vector<RetainedString> labelArguments,
        RetainedString qualifierPrefix, RetainedString qualifierSuffix)
        : iteratorArgument(std::move(iteratorArgument)), dimensionArgument(std::move(dimensionArgument)),
          kindArgument(std::move(kindArgument)), labelArguments(std::move(labelArguments)),
          qualifierPrefix(std::move(qualifierPrefix)), qualifierSuffix(std::move(qualifierSuffix)) {}
};

struct SourceSliceTypeOrigin {
    SourceSliceTypeContext qualifiedArgument;
    SourceSliceTypeContext unqualifiedArgument;
    SourceSliceTypeContext qualifiedElement;
    SourceSliceTypeContext unqualifiedElement;
};

// The defining producer supplies this record. An incomplete specialization
// intentionally supplies no identity for an arbitrary native view carrier.
template<class View> struct SourceSliceOrigin;

// A defining Source identity can be named independently of a carrier whose
// native representation has erased iterator kinds or Source qualifiers.
template<class Origin> struct SourceSliceDeclarationIdentity {};

inline nicxlive::core::d_runtime::SourceTypeSpellingNames ngSourceSliceTypeSpellings(
    const SourceSliceTypeOrigin& origin) {
    // Origin callbacks finish before entering this allocation-only boundary.
    try {
        const auto render = [](const SourceSliceTypeContext& context, bool qualified, bool argument) {
            std::string text;
            if (argument) text.push_back('(');
            text.append(context.qualifierPrefix.view());
            if (qualified) text.append("mir.ndslice.slice.");
            // Slice is an alias of this actual defining struct (slice.d669/3412).
            text.append("mir_slice!(");
            text.append(context.iteratorArgument.view());
            text.append(", ");
            text.append(context.dimensionArgument.view());
            text.append(", ");
            text.append(context.kindArgument.view());
            for (const auto& label : context.labelArguments) {
                text.append(", ");
                text.append(label.view());
            }
            text.push_back(')');
            text.append(context.qualifierSuffix.view());
            if (argument) text.push_back(')');
            return RetainedString(std::move(text));
        };
        return {render(origin.qualifiedArgument, true, true),
            render(origin.unqualifiedArgument, false, true),
            render(origin.qualifiedElement, true, false),
            render(origin.unqualifiedElement, false, false)};
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

} // namespace nijigenerate::compat

namespace nicxlive::core::d_runtime {

template<> struct SourceTypeSpellings<::nijigenerate::compat::SourceMirSliceKind> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(mir.ndslice.slice.mir_slice_kind)";
    inline static constexpr char unqualifiedArgument[] = "(mir_slice_kind)";
    inline static constexpr char qualifiedElement[] = "mir.ndslice.slice.mir_slice_kind";
    inline static constexpr char unqualifiedElement[] = "mir_slice_kind";
};

template<class Origin> requires requires { Origin::get(); }
struct SourceTypeSpellings<::nijigenerate::compat::SourceSliceDeclarationIdentity<Origin>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto origin = Origin::get();
        return ::nijigenerate::compat::ngSourceSliceTypeSpellings(origin);
    }
};

} // namespace nicxlive::core::d_runtime
