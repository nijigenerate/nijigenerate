#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <cstdint>
#include <limits>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cstddef>
#include <tuple>

namespace nijigenerate::autorig::solver::quadratic {

struct SparseEntry {
    std::int32_t row{};
    std::int32_t column{};
    double value = std::numeric_limits<double>::quiet_NaN();
};

struct QuadraticProblem {
    std::int32_t variables{};
    std::int32_t constraints{};
    compat::Slice<SparseEntry> objective;
    compat::Slice<SparseEntry> matrix;
    compat::Slice<double> linear;
    compat::Slice<double> lower;
    compat::Slice<double> upper;
};

struct SolverOptions {
    std::int32_t iterations = 50'000;
    double tolerance = 1e-7;
    double seconds = 30;
    bool polish = true;
};

struct SolverResult {
    compat::Slice<double> solution;
    std::int32_t status{};
    std::int32_t iterations{};
    double primalResidual = 0;
    double dualResidual = 0;
    double objective = 0;
    double constraintViolation = 0;

    bool solved() const { return status == 1 || status == 2; }
};

} // namespace nijigenerate::autorig::solver::quadratic

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::autorig::solver::quadratic::SparseEntry> {
    using Value = ::nijigenerate::autorig::solver::quadratic::SparseEntry;
    inline static constexpr std::string_view typeName = "SparseEntry";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("row", offsetof(Value, row), &Value::row),
            ngFormatAggregateMember("column", offsetof(Value, column), &Value::column),
            ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::autorig::solver::quadratic::QuadraticProblem> {
    using Value = ::nijigenerate::autorig::solver::quadratic::QuadraticProblem;
    inline static constexpr std::string_view typeName = "QuadraticProblem";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("variables", offsetof(Value, variables), &Value::variables),
            ngFormatAggregateMember("constraints", offsetof(Value, constraints), &Value::constraints),
            ngFormatAggregateMember("objective", offsetof(Value, objective), &Value::objective),
            ngFormatAggregateMember("matrix", offsetof(Value, matrix), &Value::matrix),
            ngFormatAggregateMember("linear", offsetof(Value, linear), &Value::linear),
            ngFormatAggregateMember("lower", offsetof(Value, lower), &Value::lower),
            ngFormatAggregateMember("upper", offsetof(Value, upper), &Value::upper)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::autorig::solver::quadratic::SolverOptions> {
    using Value = ::nijigenerate::autorig::solver::quadratic::SolverOptions;
    inline static constexpr std::string_view typeName = "SolverOptions";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("iterations", offsetof(Value, iterations), &Value::iterations),
            ngFormatAggregateMember("tolerance", offsetof(Value, tolerance), &Value::tolerance),
            ngFormatAggregateMember("seconds", offsetof(Value, seconds), &Value::seconds),
            ngFormatAggregateMember("polish", offsetof(Value, polish), &Value::polish)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::autorig::solver::quadratic::SolverResult> {
    using Value = ::nijigenerate::autorig::solver::quadratic::SolverResult;
    inline static constexpr std::string_view typeName = "SolverResult";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("solution", offsetof(Value, solution), &Value::solution),
            ngFormatAggregateMember("status", offsetof(Value, status), &Value::status),
            ngFormatAggregateMember("iterations", offsetof(Value, iterations), &Value::iterations),
            ngFormatAggregateMember("primalResidual", offsetof(Value, primalResidual), &Value::primalResidual),
            ngFormatAggregateMember("dualResidual", offsetof(Value, dualResidual), &Value::dualResidual),
            ngFormatAggregateMember("objective", offsetof(Value, objective), &Value::objective),
            ngFormatAggregateMember("constraintViolation", offsetof(Value, constraintViolation),
                &Value::constraintViolation)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::solver::quadratic::SparseEntry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.solver.quadratic.SparseEntry)";
    inline static constexpr char unqualifiedArgument[] = "(SparseEntry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.solver.quadratic.SparseEntry";
    inline static constexpr char unqualifiedElement[] = "SparseEntry";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::solver::quadratic::QuadraticProblem> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.solver.quadratic.QuadraticProblem)";
    inline static constexpr char unqualifiedArgument[] = "(QuadraticProblem)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.solver.quadratic.QuadraticProblem";
    inline static constexpr char unqualifiedElement[] = "QuadraticProblem";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::solver::quadratic::SolverOptions> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.solver.quadratic.SolverOptions)";
    inline static constexpr char unqualifiedArgument[] = "(SolverOptions)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.solver.quadratic.SolverOptions";
    inline static constexpr char unqualifiedElement[] = "SolverOptions";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::solver::quadratic::SolverResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.solver.quadratic.SolverResult)";
    inline static constexpr char unqualifiedArgument[] = "(SolverResult)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.solver.quadratic.SolverResult";
    inline static constexpr char unqualifiedElement[] = "SolverResult";
};
}
