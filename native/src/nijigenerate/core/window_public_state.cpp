// Source module state remains thread-local, including the D struct .init values.
#include "window.hpp"
#include <limits>

namespace nijigenerate::core {
nicxlive::core::DifferenceEvaluationResult ngDifferenceAggregationInitialResult() {
    nicxlive::core::DifferenceEvaluationResult value;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    value.red = nan; value.green = nan; value.blue = nan; value.alpha = nan;
    value.sampleCount = 0;
    value.tileTotals.fill(nan); value.tileCounts.fill(nan);
    return value;
}
thread_local bool ngShowStatsForNerds = false;
thread_local bool ngDifferenceAggregationDebugEnabled = false;
thread_local std::size_t ngDifferenceAggregationTargetIndex = 0;
thread_local std::size_t ngDifferenceAggregationResolvedIndex = std::numeric_limits<std::size_t>::max();
constinit thread_local nicxlive::core::DifferenceEvaluationResult ngDifferenceAggregationResult = []() constexpr {
    nicxlive::core::DifferenceEvaluationResult value{};
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    value.red = nan; value.green = nan; value.blue = nan; value.alpha = nan;
    value.sampleCount = 0;
    value.tileTotals.fill(nan); value.tileCounts.fill(nan);
    return value;
}();
thread_local bool ngDifferenceAggregationResultValid = false;
thread_local std::uint64_t ngDifferenceAggregationResultSerial = 0;
}
