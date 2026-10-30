#pragma once

#include "quadratic_sparse_sort.hpp"
#include <nijigenerate/autorig/deterministic/contracts_file.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace nijigenerate::autorig::solver::quadratic::detail {

struct Csc {
    compat::Slice<std::int32_t> pointers;
    compat::Slice<std::int32_t> rows;
    compat::Slice<double> values;
};

template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (condition) return;
    compat::RetainedString text;
    if constexpr (N > 1) text = ::nijigenerate::autorig::deterministic::contracts::detail::copyContractMessage(
        std::string_view(message, N - 1));
    else text = compat::RetainedString::fromStaticLiteral("Enforcement failed");
    throw compat::ThrowableException(text,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/solver/quadratic.d"), sourceLine);
}

// Private source body; production callers supply validated nonnegative dimensions.
inline Csc compress(compat::Slice<SparseEntry> source, std::int32_t rows,
                    std::int32_t columns, bool triangular) {
    auto entries = source.duplicate();
    for (const auto entry : entries) {
        enforce(entry.row >= 0 && entry.row < rows && entry.column >= 0 && entry.column < columns,
                "Sparse matrix index out of range",57);
        enforce(std::isfinite(entry.value), "Nonfinite sparse matrix coefficient",59);
        enforce(!triangular || entry.row <= entry.column, "Objective must contain only its upper triangle",60);
    }
    sparse_sort::sort(std::span<SparseEntry>(entries.data(), entries.size()));
    Csc result;
    const auto pointerCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(columns) + 1u);
    result.pointers.resize(static_cast<std::size_t>(pointerCount), 0);
    std::size_t index = 0;
    for (std::int32_t column = 0; column < columns; ++column) {
        result.pointers[static_cast<std::size_t>(column)] = static_cast<std::int32_t>(result.values.size());
        while (index < entries.size() && entries[index].column == column) {
            const std::int32_t row = entries[index].row;
            double value = 0;
            do { value += entries[index++].value; }
            while (index < entries.size() && entries[index].column == column && entries[index].row == row);
            enforce(std::isfinite(value), "Sparse coefficient overflow",73);
            if (value != 0) {
                result.rows.append(row);
                result.values.append(value);
            }
        }
    }
    enforce(result.values.size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),
            "Sparse matrix exceeds index range",77);
    result.pointers[static_cast<std::size_t>(columns)] = static_cast<std::int32_t>(result.values.size());
    return result;
}

} // namespace nijigenerate::autorig::solver::quadratic::detail

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::autorig::solver::quadratic::detail::Csc> {
    using Value = ::nijigenerate::autorig::solver::quadratic::detail::Csc;
    inline static constexpr std::string_view typeName = "Csc";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("pointers", offsetof(Value, pointers), &Value::pointers),
            ngFormatAggregateMember("rows", offsetof(Value, rows), &Value::rows),
            ngFormatAggregateMember("values", offsetof(Value, values), &Value::values)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::solver::quadratic::detail::Csc> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.solver.quadratic.Csc)";
    inline static constexpr char unqualifiedArgument[] = "(Csc)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.solver.quadratic.Csc";
    inline static constexpr char unqualifiedElement[] = "Csc";
};
}
