#pragma once

#include <nijigenerate/ext/nodes/exdepthnumeric.hpp>
#include <nijigenerate/ext/nodes/exdepthgrid.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nodes/node.hpp>
#include <serde_json.hpp>
#include <serde_reader.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <new>
#include <source_location>

namespace nijigenerate::ext::nodes {

class DepthMappedNode {
public:
    virtual ~DepthMappedNode() = default;
    virtual compat::Slice<float> copyDepths() = 0;
    virtual void replaceDepths(compat::Slice<float> values) = 0;
};

// Per-owner base for the argument-free D mixin.
class ExDepthMapped : public DepthMappedNode {
public:
    compat::Slice<float> depths{};

    compat::Slice<float> copyDepths() override {
        ngNormalizeDepths(std::span<float>(depths.data(), depths.size()));
        return depths.isNull() ? compat::Slice<float>{} : depths.duplicate();
    }

    void replaceDepths(compat::Slice<float> values) override {
        depths = values.isNull() ? compat::Slice<float>{} : values.duplicate();
        ngNormalizeDepths(std::span<float>(depths.data(), depths.size()));
    }

    virtual void copyDepthsFrom(std::shared_ptr<nicxlive::core::nodes::Node> source) {
        if (auto depthMapped = std::dynamic_pointer_cast<DepthMappedNode>(source)) {
            replaceDepths(depthMapped->copyDepths());
        } else depths = {};
    }

    virtual void resizeDepthsToVertices(std::size_t vertexCount) {
        if (depths.isNull()) return;
        ngNormalizeDepths(std::span<float>(depths.data(), depths.size()));
        const auto oldLength = depths.size();
        depths.resize(vertexCount);
        for (std::size_t i = oldLength; i < depths.size(); ++i) depths[i] = 0.0f;
    }

    virtual void serializeDepths(nicxlive::core::serde::TypedJsonWriter& serializer) {
        if (depths.isNull()) return;
        serializer.putKey("depths");
        const auto state = serializer.listBegin();
        const auto values = depths;
        for (float depth : values) {
            serializer.elemBegin();
            serializer.serializeValue(ngFiniteDepthOrZero(depth));
        }
        serializer.listEnd(state);
    }

    virtual nicxlive::core::serde::ReadError deserializeDepths(
        nicxlive::core::serde::TypedJsonView data, std::size_t vertexCount) {
        using namespace nicxlive::core::serde;
        const auto values = data.findFirst("depths");
        if (values.isMissing()) return {};
        if (values.kind() == JsonKind::Null) { depths = {}; return {}; }
        depths.resize(0);
        // fghj byElement yields an empty range for every non-array kind.
        for (const auto entry : values.byElement()) {
            float depth = std::numeric_limits<float>::quiet_NaN();
            if (auto error = deserializeValue(entry, depth)) return error;
            depths.append(ngFiniteDepthOrZero(depth));
        }
        if (depths.size() != vertexCount) {
            return ReadError(std::allocate_shared<ReadError::Object>(
                nicxlive::core::d_runtime::SourceAllocator<ReadError::Object>{},
                compat::RetainedString::fromStaticLiteral("depths length must match vertices length"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthmapped.d"), 248));
        }
        return {};
    }
};

} // namespace nijigenerate::ext::nodes

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::DepthMappedNode> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exdepthmapped.DepthMappedNode)";
    inline static constexpr char unqualifiedArgument[] = "(DepthMappedNode)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exdepthmapped.DepthMappedNode";
    inline static constexpr char unqualifiedElement[] = "DepthMappedNode";
};
}
