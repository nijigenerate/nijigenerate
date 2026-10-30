#include "meta_source_contours.hpp"

namespace nijigenerate::viewport::vertex::automesh::contours {

compat::RetainedString ContourAutoMeshProcessor::schema() {
    return meta::AutoMeshReflectionBody<ContourAutoMeshProcessor>(*this).schema();
}

compat::RetainedString ContourAutoMeshProcessor::values(compat::RetainedString levelName) {
    return meta::AutoMeshReflectionBody<ContourAutoMeshProcessor>(*this).values(levelName);
}

bool ContourAutoMeshProcessor::applyPreset(compat::RetainedString name) {
    return meta::AutoMeshReflectionBody<ContourAutoMeshProcessor>(*this).applyPreset(name);
}

bool ContourAutoMeshProcessor::writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) {
    return meta::AutoMeshReflectionBody<ContourAutoMeshProcessor>(*this).writeValues(levelName, updatesJson);
}

void ContourAutoMeshProcessor::configure() {
    meta::AutoMeshReflectionBody<ContourAutoMeshProcessor>(*this).configure();
}

} // namespace nijigenerate::viewport::vertex::automesh::contours
