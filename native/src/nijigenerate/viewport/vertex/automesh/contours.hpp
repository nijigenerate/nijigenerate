#pragma once

#include "automesh.hpp"
#include "meta_reflect.hpp"
#include "alpha_provider.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <memory>

namespace nijigenerate::viewport::vertex::automesh::contours {

class ContourAutoMeshProcessor : public AutoMeshProcessor, public meta::IAutoMeshReflect {
    friend struct meta::AMReflectionSource<ContourAutoMeshProcessor>;
    // D copies a descriptor of static literal data, without a module heap initializer.
    inline static constinit float initialScaleValues[]{1, 1.1f, 0.9f, 0.7f, 0.4f, 0.2f, 0.1f};
    static compat::Slice<float> initialScales() {
        // Static backing needs no control block; alias the actual array without allocation.
        return compat::Slice<float>::retainedView(
            std::shared_ptr<void>(std::shared_ptr<void>{}, initialScaleValues), initialScaleValues,
            sizeof(initialScaleValues) / sizeof(initialScaleValues[0]));
    }
public:
    using T = ContourAutoMeshProcessor;
    using This = ContourAutoMeshProcessor;
    float SAMPLING_STEP = 32;
    const float SMALL_THRESHOLD = 256;
    float maskThreshold = 15;
    float MIN_DISTANCE = 16;
    float MAX_DISTANCE = -1;
    compat::Slice<float> SCALES = initialScales();
    compat::RetainedString presetName;
    ContourAutoMeshProcessor() = default;
    ContourAutoMeshProcessor(const ContourAutoMeshProcessor&) = delete;
    ContourAutoMeshProcessor& operator=(const ContourAutoMeshProcessor&) = delete;

    static void presetNormal(std::shared_ptr<ContourAutoMeshProcessor> processor);
    static void presetDetailed(std::shared_ptr<ContourAutoMeshProcessor> processor);
    static void presetLarge(std::shared_ptr<ContourAutoMeshProcessor> processor);
    static void presetSmall(std::shared_ptr<ContourAutoMeshProcessor> processor);
    static void presetThin(std::shared_ptr<ContourAutoMeshProcessor> processor);
    static void presetEdges(std::shared_ptr<ContourAutoMeshProcessor> processor);
    Mesh autoMesh(Target target, Mesh mesh, bool mirrorHoriz = false, float axisHoriz = 0,
        bool mirrorVert = false, float axisVert = 0) override;
    virtual Mesh autoMesh(NodeSlice targets, Mesh mesh, bool mirrorHoriz = false, float axisHoriz = 0,
        bool mirrorVert = false, float axisVert = 0);
    compat::RetainedString icon() override;

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
        registerSourceClassInfo<ContourAutoMeshProcessor>(
            "nijigenerate.viewport.vertex.automesh.contours.ContourAutoMeshProcessor");
    }

private:
    AlphaPreviewState _alphaPreview;
};

} // namespace nijigenerate::viewport::vertex::automesh::contours

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.contours.ContourAutoMeshProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(ContourAutoMeshProcessor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.contours.ContourAutoMeshProcessor";
    inline static constexpr char unqualifiedElement[] = "ContourAutoMeshProcessor";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.contours.ContourAutoMeshProcessor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.contours.ContourAutoMeshProcessor)";
};
} // namespace nicxlive::core::d_runtime
