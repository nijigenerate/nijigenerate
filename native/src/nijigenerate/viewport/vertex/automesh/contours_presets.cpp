#include "contours.hpp"
#include "meta.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::viewport::vertex::automesh::contours {

void ContourAutoMeshProcessor::presetNormal(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 50;
    processor->maskThreshold = 15;
    processor->MIN_DISTANCE = 16;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Normal parts");
}
void ContourAutoMeshProcessor::presetDetailed(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 32;
    processor->maskThreshold = 15;
    processor->MIN_DISTANCE = 16;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Detailed mesh");
}
void ContourAutoMeshProcessor::presetLarge(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 80;
    processor->maskThreshold = 15;
    processor->MIN_DISTANCE = 24;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f, 0};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Large parts");
}
void ContourAutoMeshProcessor::presetSmall(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 24;
    processor->maskThreshold = 15;
    processor->MIN_DISTANCE = 12;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1, 1.1f, 0.6f, 0.2f};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Small parts");
}
void ContourAutoMeshProcessor::presetThin(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 12;
    processor->maskThreshold = 1;
    processor->MIN_DISTANCE = 4;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Thin and minimum parts");
}
void ContourAutoMeshProcessor::presetEdges(std::shared_ptr<ContourAutoMeshProcessor> processor) {
    if (!processor) compat::ngFailNullAccess(true);
    processor->SAMPLING_STEP = 24;
    processor->maskThreshold = 15;
    processor->MIN_DISTANCE = 8;
    processor->MAX_DISTANCE = processor->SAMPLING_STEP * 2;
    processor->SCALES = {1, 1.2f, 0.8f};
    processor->presetName = compat::RetainedString::fromStaticLiteral("Preserve edges");
}

compat::RetainedString ContourAutoMeshProcessor::icon() {
    return compat::RetainedString::fromStaticLiteral("\xef\x82\x97");
}
compat::RetainedString ContourAutoMeshProcessor::procId() {
    return meta::AutoMeshClassInfoBody<ContourAutoMeshProcessor>(*this).procId();
}
compat::RetainedString ContourAutoMeshProcessor::displayName() {
    return meta::AutoMeshClassInfoBody<ContourAutoMeshProcessor>(*this).displayName();
}
int ContourAutoMeshProcessor::order() {
    return meta::AutoMeshClassInfoBody<ContourAutoMeshProcessor>(*this).order();
}

} // namespace nijigenerate::viewport::vertex::automesh::contours
