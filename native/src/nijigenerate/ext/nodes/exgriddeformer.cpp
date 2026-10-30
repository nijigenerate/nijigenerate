#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>

namespace nijigenerate::ext::nodes {
using nicxlive::core::nodes::Node;

ExGridDeformer::ExGridDeformer(DeferredConstruction construction) : GridDeformer(construction) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExGridDeformer),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer"));
}

std::shared_ptr<ExGridDeformer> ExGridDeformer::create(std::shared_ptr<Node> parent) {
    return createSourceDeferred([&] { return parent; });
}

void ExGridDeformer::rebuffer(const nicxlive::core::math::Vec2Array& gridPoints) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto inputPoints = gridPoints;
    const auto oldVertices = getVertices().dup();
    auto oldDepths = copyDepths();
    auto oldOperations = copyDepthOps();
    auto oldBaseDepths = copyDepthOpBaseDepths();
    GridDeformer::rebuffer(inputPoints);
    if (!oldDepths.isNull()) {
        compat::Slice<float> resampled;
        if (ngResampleGridDepths(oldVertices, oldDepths, getVertices(), resampled)) {
            replaceDepths(resampled);
        } else {
            replaceDepths(oldDepths);
            resizeDepthsToVertices(getVertices().size());
        }
    }
    if (!oldBaseDepths.isNull()) {
        compat::Slice<float> resampled;
        if (ngResampleGridDepths(oldVertices, oldBaseDepths, getVertices(), resampled)) {
            replaceDepthOpBaseDepths(resampled);
        } else {
            replaceDepthOpBaseDepths(oldBaseDepths);
            resizeDepthOpBaseDepthsToVertices(getVertices().size());
        }
    }
    replaceDepthOps(ngRemapGridIndexBoundDepthOperations(oldVertices, getVertices(), oldOperations));
}

compat::Slice<ExDepthOp> ExGridDeformer::copyDepthOps() {
    return ExDepthOperated::copyDepthOps();
}
void ExGridDeformer::replaceDepthOps(compat::Slice<ExDepthOp> values) {
    ExDepthOperated::replaceDepthOps(values);
}
compat::Slice<float> ExGridDeformer::copyDepthOpBaseDepths() {
    return ExDepthOperated::copyDepthOpBaseDepths();
}
void ExGridDeformer::replaceDepthOpBaseDepths(compat::Slice<float> values) {
    ExDepthOperated::replaceDepthOpBaseDepths(values);
}

void ExGridDeformer::copyFrom(const Node& source, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto ownedSource = std::const_pointer_cast<Node>(source.shared_from_this());
    GridDeformer::copyFrom(source, clone, deepCopy);
    copyDepthsFrom(ownedSource);
    copyDepthOpsFrom(ownedSource);
    resizeDepthsToVertices(getVertices().size());
    resizeDepthOpBaseDepthsToVertices(getVertices().size());
}

void ExGridDeformer::serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer,
        bool recursive, nicxlive::core::nodes::SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    GridDeformer::serializeSelfImpl(serializer, recursive, flags);
    // Source helpers read this same mutable node after each preceding callback.
    auto& source = const_cast<ExGridDeformer&>(*this);
    source.serializeDepths(serializer);
    source.serializeDepthOps(serializer);
}

void ngRegisterExGridDeformer() {
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("GridDeformer"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return ExGridDeformer::create(parent);
        }));
}

nicxlive::core::serde::ReadError ExGridDeformer::deserializeFromFghj(
    nicxlive::core::serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = GridDeformer::deserializeFromFghj(data)) return error;
    if (auto error = deserializeDepths(data, getVertices().size())) return error;
    if (auto error = deserializeDepthOps(data)) return error;
    if (!depthOpBaseDepths.empty()) {
        const auto baseDepthCount = depthOpBaseDepths.size();
        const auto vertexCount = getVertices().size();
        if (baseDepthCount != vertexCount) {
            using nicxlive::core::serde::ReadError;
            return ReadError(std::allocate_shared<ReadError::Object>(
                nicxlive::core::d_runtime::SourceAllocator<ReadError::Object>{},
                compat::RetainedString::fromStaticLiteral("depth-op-base-depths length must match vertices length"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exgriddeformer.d"), 117));
        }
    }
    return {};
}

} // namespace nijigenerate::ext::nodes
