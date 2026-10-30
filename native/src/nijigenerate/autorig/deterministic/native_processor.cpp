#include "native_processor.hpp"

namespace nijigenerate::autorig::deterministic::native_detail {

RigPartMeshProcessor::RigPartMeshProcessor(float spacing, float divisions,
        compat::Slice<float> scales, bool shoulder) {
    MIN_DISTANCE = spacing;
    MASK_THRESHOLD = 1;
    DIV_PER_PART = divisions;
    LARGE_THRESHOLD = 400;
    LENGTH_THRESHOLD = 100;
    RATIO_THRESHOLD = .2f;
    SHARP_EXPANSION_FACTOR = .01f;
    NONSHARP_EXPANSION_FACTOR = .05f;
    NONSHARP_CONTRACTION_FACTOR = .05f;
    SCALES = {.5f, 0};
    if (!scales.empty()) SCALES = scales.duplicate();
    if (shoulder) {
        LARGE_THRESHOLD = 50;
        LENGTH_THRESHOLD = 20;
        RATIO_THRESHOLD = .01f;
    }
}

} // namespace nijigenerate::autorig::deterministic::native_detail
