#include "meta_source_optimum.hpp"

namespace nijigenerate::viewport::vertex::automesh::optimum {

compat::RetainedString OptimumAutoMeshProcessor::schema() {
    return meta::AutoMeshReflectionBody<OptimumAutoMeshProcessor>(*this).schema();
}

compat::RetainedString OptimumAutoMeshProcessor::values(compat::RetainedString levelName) {
    return meta::AutoMeshReflectionBody<OptimumAutoMeshProcessor>(*this).values(levelName);
}

bool OptimumAutoMeshProcessor::applyPreset(compat::RetainedString name) {
    return meta::AutoMeshReflectionBody<OptimumAutoMeshProcessor>(*this).applyPreset(name);
}

bool OptimumAutoMeshProcessor::writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) {
    return meta::AutoMeshReflectionBody<OptimumAutoMeshProcessor>(*this).writeValues(levelName, updatesJson);
}

void OptimumAutoMeshProcessor::configure() {
    meta::AutoMeshReflectionBody<OptimumAutoMeshProcessor>(*this).configure();
}

} // namespace nijigenerate::viewport::vertex::automesh::optimum
