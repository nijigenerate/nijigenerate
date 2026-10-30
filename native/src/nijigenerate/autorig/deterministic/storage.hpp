#pragma once
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <common/string_key_map.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <mutex>
#include <vector>

namespace nijigenerate::autorig::deterministic::storage {
using Json = core::settings::SettingsJsonValue;
class RigStateStorage : public framework::AutoRigWorkspace,
    public std::enable_shared_from_this<RigStateStorage> {
    using Text = compat::RetainedString;
    template<class T> using TextTable = nicxlive::core::common::PolicyKeyMap<Text,T,
        nicxlive::core::common::StringKeyPolicy>;
    struct UnsignedHash {
        std::size_t operator()(std::uint64_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    using PointBuffer = std::shared_ptr<const std::vector<contracts::Point2>>;
    using RunBuffer = std::shared_ptr<const std::vector<std::uint64_t>>;
    struct Support {
        TextTable<PointBuffer> points;
        TextTable<RunBuffer> runs;
    };
    std::recursive_mutex mutex_;
    TextTable<Support> supports;
    TextTable<Text> artifacts, names;
    nicxlive::core::common::ResourceKeyMap<std::uint64_t,Text,UnsignedHash,
        std::equal_to<std::uint64_t>> currentSupports;
    Text retain(Text name, Json value);
    Json materialMetadata(Json material, bool freshSource);
public:
    RigStateStorage();
    RigStateStorage(const RigStateStorage&) = delete;
    RigStateStorage& operator=(const RigStateStorage&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<framework::AutoRigWorkspace> other);
    virtual int opCmp(std::shared_ptr<framework::AutoRigWorkspace> other);
    virtual Json snapshot(Json state, bool freshSource = false);
    virtual Json restore(Json state);
    virtual Json artifact(Json state, compat::RetainedString name);
    Json memoryInfo() override;
    void dispose() override;
};
std::shared_ptr<RigStateStorage> ngRigStateStorage(std::shared_ptr<framework::AutoRigTaskContext> context);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::storage::RigStateStorage> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.storage.RigStateStorage)";
    inline static constexpr char unqualifiedArgument[] = "(RigStateStorage)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.storage.RigStateStorage";
    inline static constexpr char unqualifiedElement[] = "RigStateStorage";
};
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::deterministic::storage::RigStateStorage> {
    inline static constexpr char className[] = "nijigenerate.autorig.deterministic.storage.RigStateStorage";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.deterministic.storage.RigStateStorage)";
};
}
