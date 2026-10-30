#include <nijigenerate/ext/nodes/expathdeformer_node.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::ext::nodes {
using nicxlive::core::nodes::Node;
ExPathDeformer::ExPathDeformer(DeferredConstruction construction) : PathDeformer(construction) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExPathDeformer),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expathdeformer.ExPathDeformer"));
}
std::shared_ptr<ExPathDeformer> ExPathDeformer::create(std::shared_ptr<Node> parent, CurveType type) {
    return createSourceDeferred([&] { return parent; },type);
}
void ExPathDeformer::rebuffer(const nicxlive::core::math::Vec2Array& points) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto inputPoints = points;
    const auto oldVertices = getVertices().dup();
    const auto oldDepths = copyDepths();
    PathDeformer::rebuffer(inputPoints);
    if (oldDepths.isNull()) return;
    compat::Slice<float> resampled;
    if (ngResamplePathDepths(oldVertices, oldDepths, getVertices(), resampled)) {
        replaceDepths(resampled);
    } else {
        replaceDepths(oldDepths);
        resizeDepthsToVertices(getVertices().size());
    }
}
void ExPathDeformer::copyFrom(const Node& source, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto ownedSource = std::const_pointer_cast<Node>(source.shared_from_this());
    PathDeformer::copyFrom(source, clone, deepCopy);
    copyDepthsFrom(ownedSource);
    resizeDepthsToVertices(getVertices().size());
}
void ExPathDeformer::serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& writer,
    bool recursive, nicxlive::core::nodes::SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    PathDeformer::serializeSelfImpl(writer, recursive, flags);
    // The original helper reads the same node after preceding writer callbacks.
    auto& source = const_cast<ExPathDeformer&>(*this);
    source.serializeDepths(writer);
}
nicxlive::core::serde::ReadError ExPathDeformer::deserializeFromFghj(
    nicxlive::core::serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = PathDeformer::deserializeFromFghj(data)) return error;
    return deserializeDepths(data, getVertices().size());
}
void ngRegisterExPathDeformer() {
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("PathDeformer"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return ExPathDeformer::create(parent);
        }));
}
}
