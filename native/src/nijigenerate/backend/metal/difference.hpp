#pragma once
#include "context.hpp"
#include <render/common.hpp>
namespace nijigenerate::backend::metal {
struct DifferenceReduction {
    int sourceWidth{},sourceHeight{},width{},height{};
    std::vector<float> data;
};
DifferenceReduction ngReduceDifference(Image&,nicxlive::core::DifferenceEvaluationRegion);
}
