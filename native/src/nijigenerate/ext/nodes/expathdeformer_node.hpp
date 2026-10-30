#pragma once
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nodes/path_deformer.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <functional>
#include <utility>

namespace nijigenerate::ext::nodes {
class ExPathDeformer : public nicxlive::core::nodes::PathDeformer, public ExDepthMapped {
public:
    ExPathDeformer(const ExPathDeformer&) = delete;
    ExPathDeformer& operator=(const ExPathDeformer&) = delete;
    ExPathDeformer(ExPathDeformer&&) = delete;
    ExPathDeformer& operator=(ExPathDeformer&&) = delete;
    static std::shared_ptr<ExPathDeformer> create(
        std::shared_ptr<nicxlive::core::nodes::Node> parent = {}, CurveType type = CurveType::Spline);
    template<class ParentProducer> static std::shared_ptr<ExPathDeformer> createSourceDeferred(
            ParentProducer&& produceParent, CurveType type = CurveType::Spline) {
        auto result = std::allocate_shared<ExPathDeformer>(
            nicxlive::core::d_runtime::SourceAllocator<ExPathDeformer>{}, DeferredConstruction{});
        std::shared_ptr<nicxlive::core::nodes::Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        result->constructPathOwned(parent,type);
        return result;
    }
    void rebuffer(const nicxlive::core::math::Vec2Array& points) override;
    void copyFrom(const nicxlive::core::nodes::Node& source, bool clone = false, bool deepCopy = true) override;
    void serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& writer, bool recursive = true,
        nicxlive::core::nodes::SerializeNodeFlags flags = nicxlive::core::nodes::SerializeNodeFlags::All) const override;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data) override;
protected:
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    explicit ExPathDeformer(DeferredConstruction construction);
};
void ngRegisterExPathDeformer();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::ext::nodes::ExPathDeformer> {
    inline static constexpr char className[] = "nijigenerate.ext.nodes.expathdeformer.ExPathDeformer";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.nodes.expathdeformer.ExPathDeformer)";
};
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExPathDeformer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.expathdeformer.ExPathDeformer)";
    inline static constexpr char unqualifiedArgument[] = "(ExPathDeformer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.expathdeformer.ExPathDeformer";
    inline static constexpr char unqualifiedElement[] = "ExPathDeformer";
};
}
