#pragma once
// Independent nijilive OpenGL diff_collect_impl.d, BSD-2-Clause.
// Full notice: native/port/licenses/nijilive-difference-aggregation.txt.
#include <render/common.hpp>
namespace nijigenerate::backend::runtime_gl {
void oglSetDifferenceAggregationEnabled(bool enabled);
bool oglIsDifferenceAggregationEnabled();
void oglSetDifferenceAggregationRegion(nicxlive::core::DifferenceEvaluationRegion region);
nicxlive::core::DifferenceEvaluationRegion oglGetDifferenceAggregationRegion();
bool oglEvaluateDifferenceAggregation(nicxlive::core::RenderResourceHandle texture, int width, int height);
bool oglFetchDifferenceAggregationResult(nicxlive::core::DifferenceEvaluationResult& result);
}
