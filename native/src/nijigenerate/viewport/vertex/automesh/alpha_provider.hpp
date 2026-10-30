#pragma once
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include <compat/object_metadata.hpp>
#include <nodes/drawable.hpp>
#include <nodes/part.hpp>
#include <imgui.h>
#include <limits>
#include <memory>

namespace nijigenerate::viewport::vertex::automesh {
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Part;
using nicxlive::core::nodes::Drawable;
using NodeSlice = compat::Slice<std::shared_ptr<Node>>;
struct RectF {
    float x = std::numeric_limits<float>::quiet_NaN();
    float y = std::numeric_limits<float>::quiet_NaN();
    float w = std::numeric_limits<float>::quiet_NaN();
    float h = std::numeric_limits<float>::quiet_NaN();
};
struct ProjectionOptions { int padding = 8, targetWidth = 0, targetHeight = 0; };
class IAlphaProvider {
public:
    IAlphaProvider() = default;
    IAlphaProvider(const IAlphaProvider&) = delete;
    IAlphaProvider& operator=(const IAlphaProvider&) = delete;
    virtual ~IAlphaProvider() = default;
    virtual int width() = 0;
    virtual int height() = 0;
    virtual const std::uint8_t* alphaPtr() = 0;
    virtual RectF boundsWorld() = 0;
    virtual void dispose() = 0;
};
class PartAlphaProvider final : public IAlphaProvider {
public:
    explicit PartAlphaProvider(std::shared_ptr<Part> part);
    static std::shared_ptr<PartAlphaProvider> createSource(std::shared_ptr<Part> part);
    int width() override { return w_; }
    int height() override { return h_; }
    const std::uint8_t* alphaPtr() override { return alpha_.data(); }
    RectF boundsWorld() override { return bounds_; }
    void dispose() override { alpha_.resize(0); }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<IAlphaProvider> other);
    virtual int opCmp(std::shared_ptr<IAlphaProvider> other);
private:
    struct DeferredInitialization {};
    explicit PartAlphaProvider(DeferredInitialization) {}
    void initialize(std::shared_ptr<Part> part);
    compat::Slice<std::uint8_t> alpha_;
    int w_ = 0, h_ = 0;
    RectF bounds_;
};
class DeformableAlphaProvider final : public IAlphaProvider {
public:
    explicit DeformableAlphaProvider(std::shared_ptr<Node> meshGroup, ProjectionOptions opt = {});
    static std::shared_ptr<DeformableAlphaProvider> createSource(
        std::shared_ptr<Node> meshGroup, ProjectionOptions opt = {});
    int width() override { return w_; }
    int height() override { return h_; }
    const std::uint8_t* alphaPtr() override { return alpha_.data(); }
    RectF boundsWorld() override { return bounds_; }
    void dispose() override { alpha_.resize(0); }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<IAlphaProvider> other);
    virtual int opCmp(std::shared_ptr<IAlphaProvider> other);
private:
    struct DeferredInitialization {};
    explicit DeformableAlphaProvider(DeferredInitialization) {}
    void initialize(std::shared_ptr<Node> meshGroup, ProjectionOptions opt);
    compat::Slice<std::uint8_t> alpha_;
    int w_ = 0, h_ = 0;
    RectF bounds_;
};
class GenericProjectionAlphaProvider final : public IAlphaProvider {
public:
    explicit GenericProjectionAlphaProvider(NodeSlice targets, ProjectionOptions opt = {});
    static std::shared_ptr<GenericProjectionAlphaProvider> createSource(
        NodeSlice targets, ProjectionOptions opt = {});
    int width() override { return w_; }
    int height() override { return h_; }
    const std::uint8_t* alphaPtr() override { return alpha_.data(); }
    RectF boundsWorld() override { return bounds_; }
    void dispose() override { alpha_.resize(0); }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<IAlphaProvider> other);
    virtual int opCmp(std::shared_ptr<IAlphaProvider> other);
private:
    struct DeferredInitialization {};
    explicit GenericProjectionAlphaProvider(DeferredInitialization) {}
    void initialize(NodeSlice targets, ProjectionOptions opt);
    compat::Slice<std::uint8_t> alpha_;
    int w_ = 0, h_ = 0;
    RectF bounds_;
};
compat::Slice<std::shared_ptr<Drawable>> enumerateDrawablesForAutoMesh(NodeSlice targets);
struct AlphaPreviewState { bool show = false; };
void alphaPreviewDisposeTexture();
void alphaPreviewWidget(AlphaPreviewState& state, ImVec2 size = ImVec2(192, 192));
}

namespace nijigenerate::viewport::vertex::automesh::alpha_provider {
using ::nijigenerate::viewport::vertex::automesh::RectF;
using ::nijigenerate::viewport::vertex::automesh::ProjectionOptions;
using ::nijigenerate::viewport::vertex::automesh::IAlphaProvider;
using ::nijigenerate::viewport::vertex::automesh::PartAlphaProvider;
using ::nijigenerate::viewport::vertex::automesh::DeformableAlphaProvider;
using ::nijigenerate::viewport::vertex::automesh::GenericProjectionAlphaProvider;
using ::nijigenerate::viewport::vertex::automesh::enumerateDrawablesForAutoMesh;
using ::nijigenerate::viewport::vertex::automesh::AlphaPreviewState;
using ::nijigenerate::viewport::vertex::automesh::alphaPreviewDisposeTexture;
using ::nijigenerate::viewport::vertex::automesh::alphaPreviewWidget;
} // namespace nijigenerate::viewport::vertex::automesh::alpha_provider

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::RectF> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.RectF)";
    inline static constexpr char unqualifiedArgument[] = "(RectF)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.RectF";
    inline static constexpr char unqualifiedElement[] = "RectF";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::ProjectionOptions> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.ProjectionOptions)";
    inline static constexpr char unqualifiedArgument[] = "(ProjectionOptions)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.ProjectionOptions";
    inline static constexpr char unqualifiedElement[] = "ProjectionOptions";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::AlphaPreviewState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.AlphaPreviewState)";
    inline static constexpr char unqualifiedArgument[] = "(AlphaPreviewState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.AlphaPreviewState";
    inline static constexpr char unqualifiedElement[] = "AlphaPreviewState";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::IAlphaProvider> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.IAlphaProvider)";
    inline static constexpr char unqualifiedArgument[] = "(IAlphaProvider)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.IAlphaProvider";
    inline static constexpr char unqualifiedElement[] = "IAlphaProvider";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::PartAlphaProvider> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.PartAlphaProvider)";
    inline static constexpr char unqualifiedArgument[] = "(PartAlphaProvider)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.PartAlphaProvider";
    inline static constexpr char unqualifiedElement[] = "PartAlphaProvider";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::DeformableAlphaProvider> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.DeformableAlphaProvider)";
    inline static constexpr char unqualifiedArgument[] = "(DeformableAlphaProvider)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.DeformableAlphaProvider";
    inline static constexpr char unqualifiedElement[] = "DeformableAlphaProvider";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::GenericProjectionAlphaProvider> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.GenericProjectionAlphaProvider)";
    inline static constexpr char unqualifiedArgument[] = "(GenericProjectionAlphaProvider)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.GenericProjectionAlphaProvider";
    inline static constexpr char unqualifiedElement[] = "GenericProjectionAlphaProvider";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::automesh::PartAlphaProvider> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.PartAlphaProvider";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.PartAlphaProvider)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::automesh::DeformableAlphaProvider> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.DeformableAlphaProvider";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.DeformableAlphaProvider)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::automesh::GenericProjectionAlphaProvider> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.GenericProjectionAlphaProvider";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.GenericProjectionAlphaProvider)";
};
} // namespace nicxlive::core::d_runtime
