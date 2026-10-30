#include "flipconfig.hpp"
#include "detail/flipconfig_state.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <compat/source_allocator.hpp>
#include <serde_json.hpp>
#include <puppet_ext.hpp>
#include <array>
#include <limits>
#include <utility>
#include <functional>
#include <stdexcept>

namespace nijigenerate::windows::flipconfig {
using Node = nicxlive::core::nodes::Node;
using Puppet = nicxlive::core::Puppet;
namespace serde = nicxlive::core::serde;
using PairArray = compat::Slice<std::shared_ptr<FlipPair>>;
thread_local compat::RetainedStringSlice FlipConfigPath =
    compat::RetainedString::fromStaticLiteral("com.inochi2d.creator.FlipConfig");
namespace detail {
compat::Slice<std::shared_ptr<Node>> ngFlipNodes(std::shared_ptr<Puppet> puppet, std::shared_ptr<Node> root) {
    if (!root) compat::ngFailNullAccess(false);
    auto result = compat::ngRequireObject(puppet)->findNodesType<Node>(root);
    if (result.isNull()) return {};
    return compat::Slice<std::shared_ptr<Node>>::retainedView(
        result.retentionOwner(), result.data(), result.size());
}
}
namespace {
// Only passive owned-byte construction belongs inside this boundary.
template<class Build> auto allocateFlipPairText(Build&& build) {
    try { return std::forward<Build>(build)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void prepareFlipPairClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(FlipPair),
            compat::RetainedString::fromStaticLiteral("nijigenerate.windows.flipconfig.FlipPair"));
        return true;
    }();
    (void)registered;
}

compat::RetainedString nodeCString(const std::shared_ptr<Node>& node) {
    const auto receiver = compat::ngRequireObject(node);
    const auto name = receiver->getName();
    const compat::RetainedStringSlice input(name);
    const compat::SourceCString text{input};
    compat::detail::ngSourceCStringPostconditions(input.view(), text.data());
    return text.retainedString();
}
auto nodeUuid(const std::shared_ptr<Node>& node) {
    return compat::ngRequireObject(node)->getUUID();
}
bool sameUuid(const std::shared_ptr<Node>& left, const std::shared_ptr<Node>& right) {
    const auto receiver = compat::ngRequireObject(left);
    const auto leftUuid = receiver->getUUID();
    const auto rightUuid = nodeUuid(right);
    return leftUuid == rightUuid;
}
template<class Predicate> PairArray removePairs(PairArray pairs, Predicate remove) {
    std::size_t destination = 0;
    for (std::size_t i = 0; i < pairs.size(); ++i)
        // Source lambda parameters copy class handles, not their mutable cells.
        if (!remove(std::shared_ptr<FlipPair>(pairs[i]))) pairs[destination++] = pairs[i];
    pairs.resize(destination);
    return pairs;
}
bool hasNode(std::shared_ptr<FlipPair> pair, std::shared_ptr<Node> node) {
    return pair && node && ((pair->parts[0] && sameUuid(pair->parts[0], node)) ||
        (pair->parts[1] && sameUuid(pair->parts[1], node)));
}
bool matches(std::shared_ptr<FlipPair> pair, std::shared_ptr<Node> a, std::shared_ptr<Node> b) {
    return pair && a && b && pair->parts[0] && pair->parts[1] &&
        ((sameUuid(pair->parts[0], a) && sameUuid(pair->parts[1], b)) ||
         (sameUuid(pair->parts[0], b) && sameUuid(pair->parts[1], a)));
}
std::string replaceAll(std::string_view input, std::string_view from, std::string_view to) {
    return allocateFlipPairText([&] {
        std::string result;
        std::size_t offset = 0;
        for (;;) {
            const auto found = input.find(from, offset);
            if (found == input.npos) { result.append(input.substr(offset)); break; }
            result.append(input.substr(offset, found - offset)); result.append(to); offset = found + from.size();
        }
        return result;
    });
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0.
// Retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString FlipPair::toString() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    prepareFlipPairClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t FlipPair::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int FlipPair::opCmp(std::shared_ptr<FlipPair>) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    prepareFlipPairClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = allocateFlipPairText([&] {
        std::string text = "need opCmp for class ";
        text += className.view();
        return compat::RetainedString(std::move(text));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool FlipPair::opEquals(std::shared_ptr<FlipPair> other) {
    return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
}
void FlipPair::update() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(parts[0] ?
            compat::RetainedString(compat::ngRequireObject(parts[0])->getName()) :
            compat::RetainedString::fromStaticLiteral("")),
        core::translation::ngFormatArgument(parts[1] ?
            compat::RetainedString(compat::ngRequireObject(parts[1])->getName()) :
            compat::RetainedString::fromStaticLiteral(""))};
    auto formatted = core::translation::ngFormatTyped("%s <-> %s", arguments);
    name = allocateFlipPairText([&] { return compat::RetainedString(std::move(formatted)); });
}
serde::ReadError FlipPair::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = serde::deserializeValue(data.findFirst("uuid1"), uuids[0])) return error;
    if (auto error = serde::deserializeValue(data.findFirst("uuid2"), uuids[1])) return error;
    return {};
}
void FlipPair::finalize(std::shared_ptr<Puppet> puppet) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (std::dynamic_pointer_cast<ext::ExPuppet>(puppet)) {
        parts[0] = puppet->findNodeById(uuids[0]);
        parts[1] = puppet->findNodeById(uuids[1]);
    }
}
void ngLoadFlipConfig(std::shared_ptr<Puppet> puppet) {
    const auto found = compat::ngRequireObject(puppet)->extData.find(FlipConfigPath.retainedString());
    if (found == puppet->extData.end() || found->second.empty()) return;
    const auto& bytes = found->second;
    auto document = serde::parseTypedJson({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
    detail::flipPairs.resize(0);
    detail::activePuppet = puppet;
    const auto root = document.root();
    for (auto data : root.byElement()) {
        // FGHJ deserialize!Class constructs before calling the custom reader,
        // including null input, and throws the returned exception itself.
        auto pair = std::allocate_shared<FlipPair>(nicxlive::core::d_runtime::SourceAllocator<FlipPair>{});
        if (auto error = pair->deserializeFromFghj(data)) throw *error.get();
        detail::flipPairs.append(pair);
    }
    {
        const auto pairs = detail::flipPairs;
        for (auto pair : pairs) compat::ngRequireObject(pair)->reconstruct(puppet);
    }
    {
        const auto pairs = detail::flipPairs;
        for (auto pair : pairs) compat::ngRequireObject(pair)->finalize(puppet);
    }
    detail::flipPairs = removePairs(detail::flipPairs, [](const auto& pair) {
        return !compat::ngRequireObject(pair)->parts[0] && !pair->parts[1];
    });
    const auto normalizedPairs = detail::flipPairs;
    for (auto pair : normalizedPairs) if (!compat::ngRequireObject(pair)->parts[0]) {
        pair->parts[0] = pair->parts[1]; pair->parts[1].reset();
    }
}
void ngDumpFlipConfig(std::shared_ptr<Puppet> puppet) {
    if (detail::flipPairs.empty()) {
        compat::ngRequireObject(puppet)->extData.erase(FlipConfigPath.retainedString());
        return;
    }
    const auto serializer = std::allocate_shared<serde::TypedJsonWriter>(
        nicxlive::core::d_runtime::SourceAllocator<serde::TypedJsonWriter>{});
    auto state = serializer->listBegin();
    const auto pairs = detail::flipPairs;
    for (auto pair : pairs) {
        serializer->elemBegin();
        serializer->serializeValue(pair);
    }
    serializer->listEnd(state); serializer->flush();
    const auto& text = serializer->str();
    // Source casts the actual mutable appender buffer without copying bytes.
    auto* data = reinterpret_cast<std::uint8_t*>(const_cast<char*>(text.data()));
    const auto bytes = nicxlive::core::PuppetExtData::Bytes::retainedView(serializer, data, text.size());
    compat::ngRequireObject(puppet)->extData[FlipConfigPath.retainedString()] = bytes;
}
void ngInitFlipConfig() { project::incRegisterLoadFunc(&ngLoadFlipConfig); project::incRegisterSaveFunc(&ngDumpFlipConfig); }
std::shared_ptr<FlipPair> ngGetFlipPairFor(std::shared_ptr<Node> node) {
    const auto pairs = detail::flipPairs;
    for (auto pair : pairs)
        if (compat::ngRequireObject(pair)->parts[0] && pair->parts[1] &&
            (sameUuid(pair->parts[0], node) || sameUuid(pair->parts[1], node))) return pair;
    return {};
}
core::settings::SettingsJsonValue ngListFlipPairsJson() {
    using Json = core::settings::SettingsJsonValue;
    using JsonString = core::settings::SettingsString;
    core::settings::SettingsJsonArray items;
    const auto literal = []<std::size_t N>(const char (&value)[N]) {
        return JsonString(compat::RetainedString::fromStaticLiteral(value));
    };
    auto pairs = ngGetFlipPairs();
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        auto pair = pairs[i];
        core::settings::SettingsJsonObject item;
        item[literal("index")] = Json(static_cast<std::int64_t>(i));
        if (compat::ngRequireObject(pair)->parts[0]) {
            item[literal("leftUuid")] = Json(static_cast<std::int64_t>(nodeUuid(pair->parts[0])));
            const auto name = compat::ngRequireObject(pair->parts[0])->getName();
            item[literal("leftName")] = Json(JsonString(name));
            item[literal("leftType")] = Json(JsonString(compat::ngRequireObject(pair->parts[0])->typeId()));
        } else {
            item[literal("leftUuid")] = Json(static_cast<std::int64_t>(0xffffffffu));
            item[literal("leftName")] = Json(literal(""));
            item[literal("leftType")] = Json(literal(""));
        }
        if (compat::ngRequireObject(pair)->parts[1]) {
            item[literal("rightUuid")] = Json(static_cast<std::int64_t>(nodeUuid(pair->parts[1])));
            const auto name = compat::ngRequireObject(pair->parts[1])->getName();
            item[literal("rightName")] = Json(JsonString(name));
            item[literal("rightType")] = Json(JsonString(compat::ngRequireObject(pair->parts[1])->typeId()));
        } else {
            item[literal("rightUuid")] = Json(static_cast<std::int64_t>(0xffffffffu));
            item[literal("rightName")] = Json(literal(""));
            item[literal("rightType")] = Json(literal(""));
        }
        items.append(Json(item));
    }
    return Json(items);
}
FlipPairMutationResult ngReplaceFlipPairs(PairArray pairs) {
    detail::flipPairs = removePairs(pairs, [](const auto& p) { return !p || (!p->parts[0] && !p->parts[1]); });
    const auto normalizedPairs = detail::flipPairs;
    for (auto pair : normalizedPairs) {
        if (!compat::ngRequireObject(pair)->parts[0]) { pair->parts[0] = pair->parts[1]; pair->parts[1].reset(); }
        pair->update();
    }
    return {true, compat::RetainedString::fromStaticLiteral("Flip pairs replaced"), detail::flipPairs.size()};
}
FlipPairMutationResult ngAddFlipPair(std::shared_ptr<Node> left, std::shared_ptr<Node> right) {
    const auto invalid = std::numeric_limits<std::size_t>::max();
    if (!left || !right)
        return {false, compat::RetainedString::fromStaticLiteral("Both left and right nodes are required"), invalid};
    if (sameUuid(left, right))
        return {false, compat::RetainedString::fromStaticLiteral("Cannot pair a node with itself"), invalid};
    auto pairs = removePairs(ngGetFlipPairs().duplicate(), [&](const auto& p) { return !p || hasNode(p, left) || hasNode(p, right); });
    pairs.append(FlipPair::create(std::array{left, right}, compat::RetainedString::fromStaticLiteral("")));
    ngReplaceFlipPairs(pairs);
    return {true, compat::RetainedString::fromStaticLiteral("Flip pair registered"), detail::flipPairs.size() - 1};
}
FlipPairMutationResult ngRemoveFlipPair(std::shared_ptr<Node> left, std::shared_ptr<Node> right) {
    const auto invalid = std::numeric_limits<std::size_t>::max();
    if (!left || !right)
        return {false, compat::RetainedString::fromStaticLiteral("Both left and right nodes are required"), invalid};
    auto pairs = ngGetFlipPairs().duplicate();
    for (std::size_t i = 0; i < pairs.size(); ++i) if (matches(pairs[i], left, right)) {
        for (auto j = i; j + 1 < pairs.size(); ++j) pairs[j] = pairs[j + 1];
        pairs.resize(pairs.size() - 1); ngReplaceFlipPairs(pairs);
        return {true, compat::RetainedString::fromStaticLiteral("Flip pair removed"), i};
    }
    return {false, compat::RetainedString::fromStaticLiteral("Flip pair not found"), invalid};
}
FlipPairBulkMutationResult ngAutoAddFlipPairs(std::shared_ptr<Puppet> puppet,
        compat::RetainedStringSlice leftPattern, compat::RetainedStringSlice rightPattern) {
    if (!puppet) return {false, compat::RetainedString::fromStaticLiteral("No puppet"), 0};
    if (leftPattern.empty() || rightPattern.empty())
        return {false, compat::RetainedString::fromStaticLiteral("Both leftPattern and rightPattern are required"), 0};
    auto nodes = detail::ngFlipNodes(puppet, puppet->root);
    auto pairs = ngGetFlipPairs().duplicate();
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint32_t, std::uint64_t,
        UuidHash, std::equal_to<std::uint32_t>> map;
    for (std::size_t i = 0; i < pairs.size(); ++i) if (auto p = pairs[i])
        for (auto part : p->parts) if (part) map[nodeUuid(part)] = i;
    std::size_t added = 0;
    for (auto& node : nodes) {
        const auto nodeName = nodeCString(node);
        const auto targetName = replaceAll(nodeName.view(), leftPattern.view(), rightPattern.view());
        if (map.contains(nodeUuid(node)) || nodeName.view().find(leftPattern.view()) == std::string_view::npos) continue;
        for (auto& other : nodes) {
            const auto otherName = nodeCString(other);
            if (otherName.view() != std::string_view(targetName)) continue;
            const auto otherUuid = nodeUuid(other);
            const auto currentUuid = nodeUuid(node);
            if (otherUuid != currentUuid && !map.contains(nodeUuid(other))) {
                pairs.append(FlipPair::create(std::array{node, other},
                    compat::RetainedString::fromStaticLiteral("")));
                map[nodeUuid(node)] = pairs.size() - 1; map[nodeUuid(other)] = pairs.size() - 1; ++added; break;
            }
        }
    }
    ngReplaceFlipPairs(pairs);
    return {true, compat::RetainedString::fromStaticLiteral("Flip pairs registered by pattern"), added};
}
}
