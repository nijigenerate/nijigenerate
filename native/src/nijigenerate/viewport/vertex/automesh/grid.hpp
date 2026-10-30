#pragma once

#include "automesh.hpp"
#include "meta_reflect.hpp"
#include "alpha_provider.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <math/types.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::viewport::vertex::automesh::grid {

class GridAutoMeshProcessor : public AutoMeshProcessor, public meta::IAutoMeshReflect {
    friend struct meta::AMReflectionSource<GridAutoMeshProcessor>;
    // Distinct source literals share static backing across instances, never across fields.
    inline static constinit float initialScaleXValues[]{-0.1f, 0.0f, 0.5f, 1.0f, 1.1f};
    inline static constinit float initialScaleYValues[]{-0.1f, 0.0f, 0.5f, 1.0f, 1.1f};
    template<std::size_t N> static compat::Slice<float> initialScale(float (&values)[N]) {
        // Alias real static storage without allocating an artificial lifetime owner.
        return compat::Slice<float>::retainedView(
            std::shared_ptr<void>(std::shared_ptr<void>{}, values), values, N);
    }
public:
    using T = GridAutoMeshProcessor;
    using This = GridAutoMeshProcessor;
    compat::Slice<float> scaleX = initialScale(initialScaleXValues);
    compat::Slice<float> scaleY = initialScale(initialScaleYValues);
    float maskThreshold = 15;
    float xSegments = 2;
    float ySegments = 2;
    float margin = 0.1f;
    GridAutoMeshProcessor() = default;
    GridAutoMeshProcessor(const GridAutoMeshProcessor&) = delete;
    GridAutoMeshProcessor& operator=(const GridAutoMeshProcessor&) = delete;

    virtual void ngPostParamWrite(compat::RetainedString id);
    Mesh autoMesh(Target target, Mesh mesh, bool mirrorHoriz = false, float axisHoriz = 0,
        bool mirrorVert = false, float axisVert = 0) override;
    virtual void configureLegacyUI();
    compat::RetainedString icon() override;
    virtual compat::RetainedString schemaLegacy();
    virtual compat::RetainedString valuesLegacy(compat::RetainedString levelName);
    virtual bool applyPresetLegacy(compat::RetainedString name);
    virtual bool writeValuesLegacy(compat::RetainedString levelName, compat::RetainedString updatesJson);

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
        registerSourceClassInfo<GridAutoMeshProcessor>(
            "nijigenerate.viewport.vertex.automesh.grid.GridAutoMeshProcessor");
    }

private:
    AlphaPreviewState _alphaPreview;
    void rebuildScalesFromSegments();
    void rebuildGridMesh(Mesh mesh, float minX, float maxX, float minY, float maxY,
        nicxlive::core::math::Vec2 center);
};

} // namespace nijigenerate::viewport::vertex::automesh::grid

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.grid.GridAutoMeshProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(GridAutoMeshProcessor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.grid.GridAutoMeshProcessor";
    inline static constexpr char unqualifiedElement[] = "GridAutoMeshProcessor";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.grid.GridAutoMeshProcessor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.grid.GridAutoMeshProcessor)";
};
} // namespace nicxlive::core::d_runtime
