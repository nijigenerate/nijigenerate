#pragma once
#include "select.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <common/resource_key_map.hpp>
#include <render/common.hpp>
#include <array>
#include <cmath>
#include <functional>

namespace nijigenerate::viewport::common::mesheditor::tools::brush {
inline constexpr int TileColumns = 16, TileCount = TileColumns * TileColumns;
inline constexpr float OptimizationInitialStride = .1f, OptimizationFinalStrideThreshold = .01f;
inline constexpr float OptimizationParameterEpsilon = 1e-6f, OptimizationMoveEpsilon = 1e-4f;
namespace detail {
struct VertexIndexHash {
    std::size_t operator()(std::uint64_t value) const { return static_cast<std::size_t>(value); }
};
}
struct StrokeVertex {
    core::math::mesh::MeshVertex* vertex = nullptr;
    base::Vec2 startPos{NAN, NAN};
    base::Vec2 endPos{NAN, NAN};
    // Retain the actual managed vertex allocation across later optimization frames.
    core::math::mesh::MeshVertexHandle lifetime;
};
class BrushTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(BrushTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool"));
            return true;
        }();
        (void)registered;
    }
public:
    using Mat4 = nicxlive::core::math::Mat4;
    using Region = nicxlive::core::DifferenceEvaluationRegion;
    bool flow = false;
    compat::Slice<float> weights;
    Vec2 initPos{NAN, NAN};
    int axisDirection = 0;
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, Vec2,
        detail::VertexIndexHash, std::equal_to<std::uint64_t>> dragStartPositions;
    compat::Slice<std::uint64_t> draggedVertices;
    bool optimizePending = false;
    compat::Slice<StrokeVertex> strokeVertices;
    bool waitingForResult = false;
    float pendingSampleParameter = NAN;
    std::uint64_t lastResultSerial = 0;
    compat::Slice<double> bestScores;
    compat::Slice<float> bestParameters;
    Region activeRegion;
    bool activeRegionValid = false;
    int activeViewportWidth = 0, activeViewportHeight = 0;
    std::size_t currentSampleIndex = 0, currentStageSampleCount = 0;
    float currentStride = NAN, currentRangeMin = NAN, currentRangeMax = NAN;
    double stageBestScore = NAN;
    float stageBestParameter = NAN;
    double bestAggregateScore = NAN;
    float bestAggregateParameter = NAN;
    double lastSampleAggregateScore = NAN;
    bool anyVertexAdjusted = false;
    compat::Slice<double> baselineTileValues;
    double baselineGlobalScore = INFINITY;
    bool baselineValid = false;
    virtual bool getFlow() { return flow; }
    virtual void setFlow(bool value) { flow = value; }
    virtual void resetDragTracking();
    virtual void captureBaselineDifference();
    bool onDragStart(Vec2 mouse, EditorRef editor) override;
    bool onDragEnd(Vec2 mouse, EditorRef editor) override;
    bool onDragUpdate(Vec2 mouse, EditorRef editor) override { return NodeSelect::onDragUpdate(mouse, editor); }
    enum BrushActionID : std::int32_t { Drawing = int(SelectActionID::End), End };
    void setToolMode(base::VertexToolMode mode, EditorRef editor) override;
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override;
    std::int32_t unify(compat::Slice<std::int32_t> actions) override;
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    virtual void registerDraggedVertex(std::uint64_t index, core::math::mesh::MeshVertex* vertex);
    virtual void initializeStrokeOptimization(EditorRef editor);
    virtual void configureOptimizationStage();
    virtual std::size_t computeStageSampleCount(float minimum, float maximum, float stride) const;
    virtual float computeSampleParameter(std::size_t index) const;
    virtual double computeLocalContributionWeight() const;
    virtual void setStrokeParameter(EditorRef editor, float parameter);
    virtual double updateScoresForSample(EditorRef editor);
    virtual void applyBestParameters(EditorRef editor);
    virtual Mat4 computeCombinedMatrix(EditorRef editor);
    virtual double sampleDifferenceAt(Vec2 position, Mat4 combined, Mat4 camera, std::array<double, TileCount>& tiles) const;
    virtual double computeGlobalDifferenceMetric() const;
    virtual void finalizeOptimization(EditorRef editor);
    virtual bool applyOptimizationIfReady(EditorRef editor);
    virtual void dbgDrawDifferenceDiagnostics(EditorRef editor);
    void draw(base::CameraRef camera, EditorRef editor) override;
};
namespace detail { bool displayOptions(base::Editors editors); }
template<class T> requires std::derived_from<T, BrushTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.brush.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    bool viewportTools(bool deformOnly, base::VertexToolMode mode, base::Editors editors) override {
        return deformOnly ? base::ToolInfoBase<T>::viewportTools(deformOnly, mode, std::move(editors)) : false;
    }
    bool canUse(bool deformOnly, compat::Slice<base::NodeRef>) override { return deformOnly; }
    bool displayToolOptions(bool, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this(); return detail::displayOptions(std::move(editors));
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::Brush; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\x8e\xae"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Brush Tool"));
    }
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool::BrushActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool::BrushActionID;
    inline static constexpr std::string_view name = "BrushActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Drawing, "Drawing"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::StrokeVertex> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.brush.StrokeVertex)";
    inline static constexpr char unqualifiedArgument[] = "(StrokeVertex)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.brush.StrokeVertex";
    inline static constexpr char unqualifiedElement[] = "StrokeVertex";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool::BrushActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool.BrushActionID)";
    inline static constexpr char unqualifiedArgument[] = "(BrushActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool.BrushActionID";
    inline static constexpr char unqualifiedElement[] = "BrushActionID";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool)";
    inline static constexpr char unqualifiedArgument[] = "(BrushTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool";
    inline static constexpr char unqualifiedElement[] = "BrushTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.brush.BrushTool)";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.brush.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.brush.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::brush::BrushTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::brush::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.brush.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::brush::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
