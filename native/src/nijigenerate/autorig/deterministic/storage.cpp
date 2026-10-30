#include <nijigenerate/autorig/deterministic/storage.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <span>

namespace nijigenerate::autorig::deterministic::storage {
using namespace contracts;
using core::settings::SettingsJsonArray;
using core::settings::SettingsJsonObject;
using core::settings::SettingsString;
namespace {
const std::array pointKeys{compat::RetainedString::fromStaticLiteral("cloud"),
    compat::RetainedString::fromStaticLiteral("landmark_cloud"),
    compat::RetainedString::fromStaticLiteral("draw_order_cloud")};
const std::array runKeys{compat::RetainedString::fromStaticLiteral("alpha_runs_32"),
    compat::RetainedString::fromStaticLiteral("alpha_runs_128")};
const auto sourceFile=compat::RetainedString::fromStaticLiteral(
    "source/nijigenerate/autorig/deterministic/storage.d");
core::settings::SettingsJsonObject& objectOf(Json& value) {
    return value.sourceObjectRef();
}
Json jsonField(Json value,std::string_view key) {
    return value.sourceIndexValue(key);
}
template<std::size_t N> void enforce(bool condition,const char (&message)[N],std::size_t sourceLine) {
    if (!condition) throw compat::ThrowableException(
        contracts::detail::copyContractMessage(std::string_view(message,N-1)),sourceFile,sourceLine);
}

compat::RetainedString concatenate(std::string_view prefix, std::string_view suffix) {
    return contracts::detail::allocateContractStorage([&] {
        std::string bytes(prefix); bytes.append(suffix);
        return compat::RetainedString(std::move(bytes));
    });
}
template<class Map, class Key> auto tableValue(const Map& table, const Key& key, std::size_t line) {
    const auto value = table.find(key);
    if (value == table.end()) throw compat::ngStaticError<compat::RangeError>(sourceFile, line);
    return value->second;
}
Json shallowObject(Json value) {
    SettingsJsonObject result;
    const auto& source = objectOf(value);
    for (const auto& member : source) result[member.name] = member.item;
    return result;
}
void hashText(contracts::detail::Sha256& digest, std::string_view text) {
    digest.put({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}
void hashFrame(contracts::detail::Sha256& digest, std::string_view key, std::size_t length) {
    const auto frame = contracts::detail::allocateContractStorage([&] {
        return std::string(key) + ":" + std::to_string(length) + ":";
    });
    hashText(digest,frame);
}
compat::RetainedString finish(contracts::detail::Sha256& digest) {
    constexpr char hex[] = "0123456789ABCDEF";
    const auto bytes = digest.finish(); std::array<char,64> result{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        result[2 * i] = hex[bytes[i] >> 4]; result[2 * i + 1] = hex[bytes[i] & 15];
    }
    return contracts::detail::copyContractMessage(std::string_view(result.data(),result.size()));
}
}
compat::RetainedString RigStateStorage::retain(compat::RetainedString name, Json value) {
    auto serialized = core::settings::ngSettingsJsonString(value);
    const auto encoded = contracts::detail::allocateContractStorage([&] {
        return compat::RetainedString(std::move(serialized));
    });
    contracts::detail::Sha256 hash; hashText(hash, encoded.view()); auto id = finish(hash);
    if (!artifacts.contains(id)) artifacts[id] = encoded;
    names[id] = name; return id;
}
Json RigStateStorage::materialMetadata(Json material, bool freshSource) {
    auto result = shallowObject(material); auto& object = objectOf(result);
    const auto materialId = ngRigUnsigned(jsonField(material,"uuid"));
    const auto id = currentSupports.find(materialId);
    if (freshSource || id == currentSupports.end()) {
        Support support;
        for (const auto key : pointKeys) if (const auto value = object.if_contains(key.view())) {
            auto points = ngRigPoints(*value);
            PointBuffer owned;
            if (!points.empty()) owned=contracts::detail::allocateContractStorage([&] {
                return std::allocate_shared<const std::vector<Point2>>(
                    nicxlive::core::d_runtime::SourceAllocator<const std::vector<Point2>>{},points.begin(),points.end());
            });
            support.points[key]=std::move(owned);
        }
        for (const auto key : runKeys) if (const auto value = object.if_contains(key.view())) {
            std::vector<std::uint64_t> values;
            for (const auto entry : value->as_array()) {
                const auto number = ngRigUnsigned(entry);
                contracts::detail::allocateContractStorage([&] { values.push_back(number); });
            }
            RunBuffer owned;
            if(!values.empty()) owned=std::allocate_shared<const std::vector<std::uint64_t>>(
                nicxlive::core::d_runtime::SourceAllocator<const std::vector<std::uint64_t>>{},std::move(values));
            support.runs[key]=std::move(owned);
        }
        contracts::detail::Sha256 hash;
        for (const auto key : pointKeys) if (auto it = support.points.find(key); it != support.points.end()) {
            const auto length=it->second?it->second->size():0;
            hashFrame(hash,key.view(),length);
            static_assert(sizeof(Point2) == 2 * sizeof(double));
            hash.put({reinterpret_cast<const std::uint8_t*>(it->second?it->second->data():nullptr), length * sizeof(Point2)});
        }
        for (const auto key : runKeys) if (auto it = support.runs.find(key); it != support.runs.end()) {
            const auto length=it->second?it->second->size():0;
            hashFrame(hash,key.view(),length);
            hash.put({reinterpret_cast<const std::uint8_t*>(it->second?it->second->data():nullptr), length * sizeof(std::uint64_t)});
        }
        auto token = finish(hash);
        if (!supports.contains(token)) supports[token] = support;
        currentSupports[materialId] = token; object["_source_support"] = SettingsString(token);
    } else {
        enforce(supports.contains(id->second), "Missing source CPU support",66);
        object["_source_support"] = SettingsString(id->second);
    }
    for (const auto key : pointKeys) object.erase(key.view());
    for (const auto key : runKeys) object.erase(key.view());
    return result;
}
Json RigStateStorage::snapshot(Json state, bool freshSource) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    std::lock_guard lock(mutex_); auto values = shallowObject(state);
    if (auto materials = objectOf(values).if_contains("materials")) {
        SettingsJsonArray metadata;
        for (const auto material : materials->as_array()) metadata.append(materialMetadata(material, freshSource));
        objectOf(values)["materials"] = metadata;
    }
    SettingsJsonObject runtime, references;
    constexpr std::array runtimeKeys{"rootId", "source_sha256", "editor_signature", "model_sha256",
        "completed_stage", "finish_stages", "options"};
    for (const auto member : objectOf(values)) {
        bool runtimeField = false;
        for (const auto key : runtimeKeys) if (member.key() == key) runtimeField = true;
        if (runtimeField) runtime[member.name] = member.item;
        else references[member.name] = SettingsString(retain(member.name.retainedString(),member.item));
    }
    runtime["storage_schema"] = "rig-native-state-d/2"; runtime["artifact_refs"] = references; return runtime;
}
Json RigStateStorage::restore(Json state) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    std::lock_guard lock(mutex_);
    enforce(ngRigString(state, compat::RetainedString::fromStaticLiteral("storage_schema"),
        compat::RetainedString::emptyLiteral()).view() ==
        "rig-native-state-d/2", "Unsupported native rig state storage",102);
    auto result = shallowObject(state); auto& object = objectOf(result);
    object.erase("artifact_refs"); object.erase("storage_schema");
    auto sourceReferences = jsonField(state,"artifact_refs");
    for (const auto member : objectOf(sourceReferences)) {
        const auto encoded = artifacts.find(member.item.as_string().retainedString());
        if(encoded==artifacts.end()) {
            const auto message = concatenate("Missing native rig artifact: ",member.key());
            throw compat::ThrowableException(contracts::detail::copyContractMessage(message.view()),sourceFile,109);
        }
        object[member.name] = json::ngParseAutoRigJson(encoded->second);
    }
    if (auto materials = object.if_contains("materials")) for (auto& material : materials->as_array()) {
        const auto token=jsonField(material,"_source_support").as_string().retainedString();
        const auto support = supports.find(token); enforce(support != supports.end(), "Missing source CPU support",115);
        currentSupports[ngRigUnsigned(jsonField(material,"uuid"))] = token;
        auto& fields=objectOf(material);fields.erase("_source_support");
        const auto evaluatedSupport = support->second;
        for (const auto [key, points] : evaluatedSupport.points) {
            SettingsJsonArray rows;
            if(points) for (const auto& point : *points) rows.append(SettingsJsonArray{Json(point[0]), Json(point[1])});
            fields[SettingsString(key)] = rows;
        }
        for (const auto [key, runs] : evaluatedSupport.runs) {
            SettingsJsonArray values;
            if(runs) for (const auto value : *runs) values.append(Json(value));
            fields[SettingsString(key)] = values;
        }
    }
    return result;
}
Json RigStateStorage::artifact(Json state, compat::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    std::lock_guard lock(mutex_);
    auto sourceReferences = jsonField(state,"artifact_refs");
    const auto reference = objectOf(sourceReferences).if_contains(name.view());
    if(!reference) {
        const auto message = concatenate("Missing native rig artifact reference: ",name.view());
        throw compat::ThrowableException(contracts::detail::copyContractMessage(message.view()),sourceFile,134);
    }
    return json::ngParseAutoRigJson(tableValue(artifacts,reference->as_string().retainedString(),135));
}
Json RigStateStorage::memoryInfo() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    std::lock_guard lock(mutex_); std::uint64_t jsonBytes = 0, cpuBytes = 0; SettingsJsonArray entries;
    const auto evaluatedArtifacts = artifacts;
    for (const auto [id, value] : evaluatedArtifacts) {
        jsonBytes += value.size(); SettingsJsonObject entry;
        entry["id"] = SettingsString(id); entry["name"] = SettingsString(tableValue(names,id,145)); entry["bytes"] = Json(static_cast<std::uint64_t>(value.size()));
        entries.append(entry);
    }
    const auto evaluatedSupports = supports;
    for (const auto [id, support] : evaluatedSupports) {
        for (const auto [key, points] : support.points) cpuBytes += (points?points->size():0) * sizeof(Point2);
        for (const auto [key, runs] : support.runs) cpuBytes += (runs?runs->size():0) * sizeof(std::uint64_t);
    }
    SettingsJsonObject result; result["json_bytes"] = Json(jsonBytes); result["cpu_bytes"] = Json(cpuBytes);
    result["source_support_count"] = Json(static_cast<std::uint64_t>(supports.size())); result["artifacts"] = entries;
    return result;
}
void RigStateStorage::dispose() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    std::lock_guard lock(mutex_); supports={}; artifacts={}; names={}; currentSupports={};
}
RigStateStorage::RigStateStorage() {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RigStateStorage),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.autorig.deterministic.storage.RigStateStorage"));
}
compat::RetainedString RigStateStorage::toString() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t RigStateStorage::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool RigStateStorage::opEquals(std::shared_ptr<framework::AutoRigWorkspace> other) {
    return other && dynamic_cast<const void*>(other.get()) == dynamic_cast<const void*>(this);
}
int RigStateStorage::opCmp(std::shared_ptr<framework::AutoRigWorkspace>) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = concatenate("need opCmp for class ",className.view());
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
std::shared_ptr<RigStateStorage> ngRigStateStorage(std::shared_ptr<framework::AutoRigTaskContext> context) {
    return std::dynamic_pointer_cast<RigStateStorage>(compat::ngRequireObject(context)->workspace(
        compat::RetainedString::fromStaticLiteral("rig-state"),
        [] { return std::allocate_shared<RigStateStorage>(
            nicxlive::core::d_runtime::SourceAllocator<RigStateStorage>{}); }));
}
}
