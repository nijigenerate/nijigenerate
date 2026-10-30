#include "meta_source_skeletonize.hpp"

namespace nijigenerate::viewport::vertex::automesh::skeletonize {

compat::RetainedString SkeletonExtractor::schema() {
    return meta::AutoMeshReflectionBody<SkeletonExtractor>(*this).schema();
}

compat::RetainedString SkeletonExtractor::values(compat::RetainedString levelName) {
    return meta::AutoMeshReflectionBody<SkeletonExtractor>(*this).values(levelName);
}

bool SkeletonExtractor::applyPreset(compat::RetainedString name) {
    return meta::AutoMeshReflectionBody<SkeletonExtractor>(*this).applyPreset(name);
}

bool SkeletonExtractor::writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) {
    return meta::AutoMeshReflectionBody<SkeletonExtractor>(*this).writeValues(levelName, updatesJson);
}

void SkeletonExtractor::configure() {
    meta::AutoMeshReflectionBody<SkeletonExtractor>(*this).configure();
}

} // namespace nijigenerate::viewport::vertex::automesh::skeletonize
