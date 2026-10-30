#include "nijigenerate/autorig/deterministic/linear.hpp"
#include "contracts_file.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include <cmath>
#include <cstddef>
#include <utility>
#include <string>
#include <limits>
#include <vector>

namespace nijigenerate::autorig::deterministic::linear {
namespace {
template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (!condition) throw compat::ThrowableException{
        contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/linear.d"), sourceLine};
}
}

RigMatrix ngRigSolve(RigMatrix matrix, RigMatrix right) {
    const std::size_t n = matrix.size();
    enforce(n > 0 && right.size() == n, "Invalid linear system dimensions", 9);
    const std::size_t outputs = right[0].size();
    RigMatrix a, b;
    double scale = 0;
    for (auto row : matrix) {
        enforce(row.size() == n, "Expected a square linear system", 14);
        a.append(row.duplicate());
        for (const double value : row) {
            enforce(std::isfinite(value), "Nonfinite linear coefficient", 16);
            if (std::fabs(value) > scale) scale = std::fabs(value);
        }
    }
    for (auto row : right) {
        enforce(row.size() == outputs, "Linear right-hand dimensions differ", 19);
        b.append(row.duplicate());
        for (const double value : row) enforce(std::isfinite(value), "Nonfinite linear target", 21);
    }
    enforce(scale > 0, "Singular linear system", 23);
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::fabs(a[i][k]) > std::fabs(a[pivot][k])) pivot = i;
        enforce(std::fabs(a[pivot][k]) > scale * 1e-13, "Singular or ill-conditioned linear system", 27);
        std::swap(a[k], a[pivot]);
        std::swap(b[k], b[pivot]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double factor = a[i][k] / a[k][k];
            for (std::size_t j = k; j < n; ++j) a[i][j] -= factor * a[k][j];
            for (std::size_t j = 0; j < outputs; ++j) b[i][j] -= factor * b[k][j];
        }
    }
    auto result = ngRigZeroMatrix(n, outputs);
    for (std::size_t reverse = n; reverse != 0; --reverse) {
        const auto i = reverse - 1;
        for (std::size_t j = 0; j < outputs; ++j) {
            double value = b[i][j];
            for (std::size_t k = i + 1; k < n; ++k) value -= a[i][k] * result[k][j];
            result[i][j] = value / a[i][i];
            enforce(std::isfinite(result[i][j]), "Linear solution overflow", 41);
        }
    }
    return result;
}

RigMatrix ngRigZeroMatrix(std::size_t rows, std::size_t columns) {
    RigMatrix result;
    for (std::size_t i = 0; i < rows; ++i) {
        // Configured druntime returns null for new double[0].
        RigRow row;
        row.resize(columns, 0.0);
        result.append(row);
    }
    return result;
}

RigMatrix ngRigLeastSquares(RigMatrix matrix, RigMatrix target) {
    const auto rows = matrix.size();
    if (matrix.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, matrix.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/linear.d"), 54);
    const auto columns = matrix[0].size();
    if (target.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, target.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/linear.d"), 54);
    const auto outputs = target[0].size();
    enforce(rows >= columns && target.size() == rows, "Underdetermined landmark fit", 55);
    RigMatrix a, b;
    for (auto row : matrix) {
        enforce(row.size() == columns, "Invalid least-squares matrix", 57);
        a.append(row.duplicate());
    }
    for (auto row : target) {
        enforce(row.size() == outputs, "Invalid least-squares target", 58);
        b.append(row.duplicate());
    }
    for (std::size_t k = 0; k < columns; ++k) {
        double norm = 0;
        for (std::size_t i = k; i < rows; ++i) norm += a[i][k] * a[i][k];
        norm = std::sqrt(norm);
        enforce(std::isfinite(norm) && norm > 1e-12, "Rank-deficient landmark fit", 63);
        const double alpha = a[k][k] >= 0 ? -norm : norm;
        RigRow v;
        v.resize(rows - k, std::numeric_limits<double>::quiet_NaN());
        for (std::size_t i = k; i < rows; ++i) v[i - k] = a[i][k];
        v[0] -= alpha;
        double denominator = 0;
        for (const double value : v) denominator += value * value;
        for (std::size_t j = k; j < columns; ++j) {
            double dot = 0;
            for (std::size_t i = k; i < rows; ++i) dot += v[i - k] * a[i][j];
            for (std::size_t i = k; i < rows; ++i) a[i][j] -= 2 * v[i - k] * dot / denominator;
        }
        for (std::size_t j = 0; j < outputs; ++j) {
            double dot = 0;
            for (std::size_t i = k; i < rows; ++i) dot += v[i - k] * b[i][j];
            for (std::size_t i = k; i < rows; ++i) b[i][j] -= 2 * v[i - k] * dot / denominator;
        }
    }
    auto result = ngRigZeroMatrix(columns, outputs);
    for (std::size_t reverse = columns; reverse != 0; --reverse) {
        const auto i = reverse - 1;
        for (std::size_t j = 0; j < outputs; ++j) {
            double value = b[i][j];
            for (std::size_t k = i + 1; k < columns; ++k) value -= a[i][k] * result[k][j];
            result[i][j] = value / a[i][i];
            enforce(std::isfinite(result[i][j]), "Least-squares solution overflow", 86);
        }
    }
    return result;
}
} // namespace nijigenerate::autorig::deterministic::linear
