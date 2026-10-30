#pragma once

#include "automesh.hpp"
#include "meta_reflect.hpp"
#include "alpha_provider.hpp"
#include "nijigenerate/compat/mir_any.hpp"
#include "nijigenerate/compat/source_mir_predicate.hpp"
#include "nijigenerate/compat/phobos_ranges.hpp"
#include <compat/object_metadata.hpp>
#include <memory>

namespace nijigenerate::viewport::vertex::automesh::optimum {

template<auto Predicate>
inline constexpr ::nijigenerate::compat::phobos::Filter<Predicate> stdFilter{};
template<auto Predicate = ::nijigenerate::compat::phobos::detail::Equal{}>
inline constexpr ::nijigenerate::compat::phobos::Uniq<Predicate> stdUniq{};

// Source aliases use the actual range/callable declaration providers below.
// A native iterator API or callable type name does not supply those contracts.
template<auto Predicate = ::nijigenerate::compat::mir::detail::Identity{}>
inline constexpr ::nijigenerate::compat::mir::Any<Predicate> mirAny{};

class OptimumAutoMeshProcessor : public AutoMeshProcessor, public meta::IAutoMeshReflect {
    friend struct meta::AMReflectionSource<OptimumAutoMeshProcessor>;
    // Each instance receives the source class initializer's static literal descriptor.
    inline static constinit float initialScaleValues[]{0.5f, 0.0f};
    static compat::Slice<float> initialScales() {
        return compat::Slice<float>::retainedView(
            std::shared_ptr<void>(std::shared_ptr<void>{}, initialScaleValues), initialScaleValues,
            sizeof(initialScaleValues) / sizeof(initialScaleValues[0]));
    }
public:
    using T = OptimumAutoMeshProcessor;
    using This = OptimumAutoMeshProcessor;
    float LARGE_THRESHOLD = 400;
    float LENGTH_THRESHOLD = 100;
    float RATIO_THRESHOLD = 0.2f;
    float SHARP_EXPANSION_FACTOR = 0.01f;
    float NONSHARP_EXPANSION_FACTOR = 0.05f;
    float NONSHARP_CONTRACTION_FACTOR = 0.05f;
    compat::Slice<float> SCALES = initialScales();
    float MIN_DISTANCE = 10;
    float MASK_THRESHOLD = 1;
    float DIV_PER_PART = 12;
    compat::RetainedString presetName;
    OptimumAutoMeshProcessor() = default;
    OptimumAutoMeshProcessor(const OptimumAutoMeshProcessor&) = delete;
    OptimumAutoMeshProcessor& operator=(const OptimumAutoMeshProcessor&) = delete;
    Mesh autoMesh(Target target, Mesh mesh, bool mirrorHoriz = false, float axisHoriz = 0,
        bool mirrorVert = false, float axisVert = 0) override;
    compat::RetainedString icon() override;
    static void presetNormal(std::shared_ptr<OptimumAutoMeshProcessor> processor);
    static void presetDetailed(std::shared_ptr<OptimumAutoMeshProcessor> processor);
    static void presetLarge(std::shared_ptr<OptimumAutoMeshProcessor> processor);
    static void presetSmall(std::shared_ptr<OptimumAutoMeshProcessor> processor);
    static void presetThin(std::shared_ptr<OptimumAutoMeshProcessor> processor);
    static void presetEdges(std::shared_ptr<OptimumAutoMeshProcessor> processor);
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
        registerSourceClassInfo<OptimumAutoMeshProcessor>(
            "nijigenerate.viewport.vertex.automesh.optimum.OptimumAutoMeshProcessor");
    }

private:
    static compat::RetainedString _fallbackId() {
        return compat::RetainedString::fromStaticLiteral("OptimumAutoMeshProcessor");
    }
    AlphaPreviewState _alphaPreview;
};

} // namespace nijigenerate::viewport::vertex::automesh::optimum

namespace nicxlive::core::d_runtime {

template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.optimum.OptimumAutoMeshProcessor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.optimum.OptimumAutoMeshProcessor)";
};

template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.optimum.OptimumAutoMeshProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(OptimumAutoMeshProcessor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.optimum.OptimumAutoMeshProcessor";
    inline static constexpr char unqualifiedElement[] = "OptimumAutoMeshProcessor";
};

} // namespace nicxlive::core::d_runtime
