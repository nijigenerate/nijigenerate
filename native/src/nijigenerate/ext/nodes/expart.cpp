#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <serde_reader.hpp>
#include <optional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::ext::nodes {
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Part;
using nicxlive::core::nodes::MeshData;
using nicxlive::core::Texture;
using namespace nicxlive::core::serde;

namespace {
template<class Operation> decltype(auto) allocateExPartArray(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

ExPart::ExPart() : Part() {
    uuid = Node::inCreateUUID();
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExPart),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expart.ExPart"));
}

ExPart::ExPart(DeferredConstruction construction) : Part(construction) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExPart),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expart.ExPart"));
}

std::shared_ptr<ExPart> ExPart::create(std::shared_ptr<Node> parent) {
    auto result = std::allocate_shared<ExPart>(
        nicxlive::core::d_runtime::SourceAllocator<ExPart>{}, DeferredConstruction{});
    result->constructPartOwned(parent);
    return result;
}

std::shared_ptr<ExPart> ExPart::create(MeshData data,
    nicxlive::core::common::SharedArray<std::shared_ptr<Texture>> textures,
    std::shared_ptr<Node> parent) {
    auto result = std::allocate_shared<ExPart>(
        nicxlive::core::d_runtime::SourceAllocator<ExPart>{}, DeferredConstruction{});
    result->constructSourceData(data, textures, parent);
    return result;
}

void ExPart::constructSourceData(const MeshData& data,
    const nicxlive::core::common::SharedArray<std::shared_ptr<Texture>>& textures,
    const std::shared_ptr<Node>& parent) {
    const auto id = Node::inCreateUUID();
    constructPartDataOwned(data, textures, id, parent);
}

ExPart::ExPart(const MeshData& data, const std::vector<std::shared_ptr<Texture>>& textures)
    : Part(data, textures, Node::inCreateUUID()) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExPart),
        compat::RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expart.ExPart"));
}

void ExPart::serializeSelf(TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Part::serializeSelf(serializer);
    serializer.putKey("psdLayerPath");
    const auto currentLayerPath = layerPath;
    serializer.putValue(currentLayerPath);
}

ReadError ExPart::deserializeFromFghj(TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = Part::deserializeFromFghj(data)) return error;
    const auto value = data.findFirst("psdLayerPath");
    // The actual Source string decoder preserves its field on returned error;
    // the authored caller deliberately ignores that return value.
    if (!value.isMissing()) (void)deserializeValue(value, layerPath);
    return {};
}

void ExPart::serializeSelf(InochiSerializer& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Part::serializeSelf(serializer);
    serializer.putKey("psdLayerPath");
    const auto currentLayerPath = layerPath;
    serializer.putValue(currentLayerPath.isNull() ? std::string{} : std::string(currentLayerPath.view()));
}

SerdeException ExPart::deserializeFromFghj(const Fghj& data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = Part::deserializeFromFghj(data)) return error;
    if (const auto value = data.get_optional<std::string>("psdLayerPath"))
        layerPath = compat::RetainedString(*value);
    return {};
}

std::shared_ptr<ExPart> incCreateExPart(std::shared_ptr<Texture> texture,
    std::shared_ptr<Node> parent, nicxlive::core::common::NullableString name) {
    MeshData data;
    data.vertices = {
        {static_cast<float>(-(compat::ngRequireObject(texture)->width() / 2)),
            static_cast<float>(-(compat::ngRequireObject(texture)->height() / 2))},
        {static_cast<float>(-(compat::ngRequireObject(texture)->width() / 2)),
            static_cast<float>(compat::ngRequireObject(texture)->height() / 2)},
        {static_cast<float>(compat::ngRequireObject(texture)->width() / 2),
            static_cast<float>(-(compat::ngRequireObject(texture)->height() / 2))},
        {static_cast<float>(compat::ngRequireObject(texture)->width() / 2),
            static_cast<float>(compat::ngRequireObject(texture)->height() / 2)}};
    data.uvs = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    allocateExPartArray([&] { data.indices = {0, 1, 2, 2, 1, 3}; });
    auto part = ExPart::createSourceData([&] {
        auto dataArgument = data;
        auto texturesArgument = allocateExPartArray([&] {
            return nicxlive::core::common::SharedArray<std::shared_ptr<Texture>>{texture};
        });
        auto parentArgument = parent;
        return std::tuple{std::move(dataArgument), std::move(texturesArgument), std::move(parentArgument)};
    });
    part->setName(name);
    return part;
}

void incRegisterExPart() {
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("Part"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) -> std::shared_ptr<Node> {
            return ExPart::create(parent);
        }));
}
} // namespace nijigenerate::ext::nodes
