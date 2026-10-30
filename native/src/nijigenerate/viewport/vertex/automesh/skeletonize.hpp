#pragma once

#include "automesh.hpp"
#include "meta_reflect.hpp"
#include "alpha_provider.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/vec2u.hpp"
#include <cstdint>

namespace nijigenerate::viewport::vertex::automesh::skeletonize {

using Point = ::nijigenerate::compat::Vec2u;
using PointArray = ::nijigenerate::compat::Slice<Point>;

class SkeletonExtractor : public AutoMeshProcessor, public meta::IAutoMeshReflect {
    friend struct meta::AMReflectionSource<SkeletonExtractor>;
public:
    using T = SkeletonExtractor;
    using This = SkeletonExtractor;
    float maskThreshold = 15;
    float targetPointCount = 10;
    SkeletonExtractor() = default;
    SkeletonExtractor(const SkeletonExtractor&) = delete;
    SkeletonExtractor& operator=(const SkeletonExtractor&) = delete;

    Mesh autoMesh(Target target, Mesh mesh, bool mirrorHoriz = false, float axisHoriz = 0,
        bool mirrorVert = false, float axisVert = 0) override;
    virtual PointArray getControlPoints();
    compat::RetainedString icon() override;

    // Concrete expansion of this class's source metadata/reflection mixins.
    compat::RetainedString procId() override;
    compat::RetainedString displayName() override;
    int order() override;
    void configure() override;
    compat::RetainedString schema() override;
    compat::RetainedString values(compat::RetainedString levelName) override;
    bool applyPreset(compat::RetainedString name) override;
    bool writeValues(compat::RetainedString levelName, compat::RetainedString updatesJson) override;

protected:
    void prepareSourceClassInfo() override {
        AutoMeshProcessor::prepareSourceClassInfo();
        registerSourceClassInfo<SkeletonExtractor>(
            "nijigenerate.viewport.vertex.automesh.skeletonize.SkeletonExtractor");
    }

private:
    PointArray controlPoints;
    AlphaPreviewState _alphaPreview;
    PointArray simplifyByTargetCount(PointArray points, std::int32_t targetCount);
};

} // namespace nijigenerate::viewport::vertex::automesh::skeletonize

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.skeletonize.SkeletonExtractor)";
    inline static constexpr char unqualifiedArgument[] = "(SkeletonExtractor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.skeletonize.SkeletonExtractor";
    inline static constexpr char unqualifiedElement[] = "SkeletonExtractor";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.skeletonize.SkeletonExtractor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.skeletonize.SkeletonExtractor)";
};
} // namespace nicxlive::core::d_runtime
