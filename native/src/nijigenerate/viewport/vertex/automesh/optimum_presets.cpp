#include "optimum.hpp"
#include "meta.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"

namespace nijigenerate::viewport::vertex::automesh::optimum {

void OptimumAutoMeshProcessor::presetNormal(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 15;
    processor->DIV_PER_PART = 12;
    processor->MIN_DISTANCE = 16;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->LARGE_THRESHOLD = 400;
    processor->LENGTH_THRESHOLD = 100;
    processor->RATIO_THRESHOLD = 0.20f;
    processor->SHARP_EXPANSION_FACTOR = 0.010f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.050f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.050f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Normal parts");
}
void OptimumAutoMeshProcessor::presetDetailed(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 15;
    processor->DIV_PER_PART = 16;
    processor->MIN_DISTANCE = 12;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->LARGE_THRESHOLD = 380;
    processor->LENGTH_THRESHOLD = 90;
    processor->RATIO_THRESHOLD = 0.18f;
    processor->SHARP_EXPANSION_FACTOR = 0.008f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.045f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.045f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Detailed mesh");
}
void OptimumAutoMeshProcessor::presetLarge(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 15;
    processor->DIV_PER_PART = 8;
    processor->MIN_DISTANCE = 24;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->LARGE_THRESHOLD = 600;
    processor->LENGTH_THRESHOLD = 200;
    processor->RATIO_THRESHOLD = 0.30f;
    processor->SHARP_EXPANSION_FACTOR = 0.015f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.080f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.080f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Large parts");
}
void OptimumAutoMeshProcessor::presetSmall(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 15;
    processor->DIV_PER_PART = 18;
    processor->MIN_DISTANCE = 12;
    processor->SCALES = {1, 1.1f, 0.6f, 0.2f};
    processor->LARGE_THRESHOLD = 300;
    processor->LENGTH_THRESHOLD = 80;
    processor->RATIO_THRESHOLD = 0.15f;
    processor->SHARP_EXPANSION_FACTOR = 0.008f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.040f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.040f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Small parts");
}
void OptimumAutoMeshProcessor::presetThin(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 1;
    processor->DIV_PER_PART = 24;
    processor->MIN_DISTANCE = 4;
    processor->SCALES = {1};
    processor->LARGE_THRESHOLD = 200;
    processor->LENGTH_THRESHOLD = 60;
    processor->RATIO_THRESHOLD = 0.10f;
    processor->SHARP_EXPANSION_FACTOR = 0.006f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.030f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.030f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Thin and minimum parts");
}
void OptimumAutoMeshProcessor::presetEdges(std::shared_ptr<OptimumAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->MASK_THRESHOLD = 15;
    processor->DIV_PER_PART = 20;
    processor->MIN_DISTANCE = 8;
    processor->SCALES = {1, 1.2f, 0.8f};
    processor->LARGE_THRESHOLD = 350;
    processor->LENGTH_THRESHOLD = 80;
    processor->RATIO_THRESHOLD = 0.15f;
    processor->SHARP_EXPANSION_FACTOR = 0.010f;
    processor->NONSHARP_EXPANSION_FACTOR = 0.030f;
    processor->NONSHARP_CONTRACTION_FACTOR = 0.030f;
    processor->presetName = compat::RetainedString::fromStaticLiteral("Preserve edges");
}

compat::RetainedString OptimumAutoMeshProcessor::icon() {
    return compat::RetainedString::fromStaticLiteral("\xee\x87\x84");
}
compat::RetainedString OptimumAutoMeshProcessor::procId() {
    return meta::AutoMeshClassInfoBody<OptimumAutoMeshProcessor>(*this).procId();
}
compat::RetainedString OptimumAutoMeshProcessor::displayName() {
    return meta::AutoMeshClassInfoBody<OptimumAutoMeshProcessor>(*this).displayName();
}
int OptimumAutoMeshProcessor::order() {
    return meta::AutoMeshClassInfoBody<OptimumAutoMeshProcessor>(*this).order();
}

} // namespace nijigenerate::viewport::vertex::automesh::optimum
