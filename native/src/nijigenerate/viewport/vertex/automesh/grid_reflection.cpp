#include "meta_source_grid.hpp"

namespace nijigenerate::viewport::vertex::automesh::grid {

compat::RetainedString GridAutoMeshProcessor::schema() {
    return meta::AutoMeshReflectionBody<GridAutoMeshProcessor>(*this).schema();
}

compat::RetainedString GridAutoMeshProcessor::values(compat::RetainedString levelName) {
    return meta::AutoMeshReflectionBody<GridAutoMeshProcessor>(*this).values(levelName);
}

bool GridAutoMeshProcessor::applyPreset(compat::RetainedString name) {
    return meta::AutoMeshReflectionBody<GridAutoMeshProcessor>(*this).applyPreset(name);
}

bool GridAutoMeshProcessor::writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) {
    return meta::AutoMeshReflectionBody<GridAutoMeshProcessor>(*this).writeValues(levelName, updatesJson);
}

void GridAutoMeshProcessor::configure() {
    meta::AutoMeshReflectionBody<GridAutoMeshProcessor>(*this).configure();
}

} // namespace nijigenerate::viewport::vertex::automesh::grid
