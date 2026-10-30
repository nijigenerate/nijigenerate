#pragma once

#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/ext/nodes/exdepthops.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <functional>
#include <utility>

namespace nijigenerate::ext::nodes {

// Each accepted mixin contributes exactly one per-owner state/helper base.
class ExGridDeformer : public nicxlive::core::nodes::GridDeformer,
    public ExDepthMapped, public DepthOperationMappedNode, public ExDepthOperated {
public:
    static std::shared_ptr<ExGridDeformer> create(
        std::shared_ptr<nicxlive::core::nodes::Node> parent = {});
    template<class ParentProducer> static std::shared_ptr<ExGridDeformer> createSourceDeferred(ParentProducer&& produceParent) {
        auto result = std::allocate_shared<ExGridDeformer>(
            nicxlive::core::d_runtime::SourceAllocator<ExGridDeformer>{}, DeferredConstruction{});
        std::shared_ptr<nicxlive::core::nodes::Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        result->constructGridOwned(parent);
        return result;
    }
    ExGridDeformer(const ExGridDeformer&) = delete;
    ExGridDeformer& operator=(const ExGridDeformer&) = delete;
    ExGridDeformer(ExGridDeformer&&) = delete;
    ExGridDeformer& operator=(ExGridDeformer&&) = delete;
    compat::Slice<ExDepthOp> copyDepthOps() override;
    void replaceDepthOps(compat::Slice<ExDepthOp> values) override;
    compat::Slice<float> copyDepthOpBaseDepths() override;
    void replaceDepthOpBaseDepths(compat::Slice<float> values) override;

    void rebuffer(const nicxlive::core::math::Vec2Array& gridPoints) override;
    void copyFrom(const nicxlive::core::nodes::Node& source, bool clone = false, bool deepCopy = true) override;
    nicxlive::core::serde::ReadError deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) override;

    void serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        nicxlive::core::nodes::SerializeNodeFlags flags = nicxlive::core::nodes::SerializeNodeFlags::All) const override;
protected:
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    explicit ExGridDeformer(DeferredConstruction construction);
};

void ngRegisterExGridDeformer();

} // namespace nijigenerate::ext::nodes

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExGridDeformer> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExGridDeformer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer)";
    inline static constexpr char unqualifiedArgument[] = "(ExGridDeformer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer";
    inline static constexpr char unqualifiedElement[] = "ExGridDeformer";
};
}
