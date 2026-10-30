#pragma once

#include <nijigenerate/ext/nodes/exdepthbone.hpp>
#include <serde_reader.hpp>
#include <serde_source_exceptions.hpp>
#include <compat/source_allocator.hpp>
#include <common/resource_key_map.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::ext::nodes {

// Nullable D associative-array handle. Copies share initialized storage; copying
// a null handle does not make subsequent initialization visible to that copy.
class DepthUuidFloatMap {
public:
    using Storage = nicxlive::core::common::ResourceKeyMap<std::uint64_t, float,
        detail::DepthRigUuidHash, std::equal_to<std::uint64_t>>;
    class ConstLookup {
        friend class DepthUuidFloatMap;
        std::optional<const Storage::MappedReference> retained_;
        const float* value_ = nullptr;

        explicit ConstLookup(Storage::MappedReference entry) : retained_(std::in_place, entry) {
            value_ = std::addressof(static_cast<float&>(*retained_));
        }
    public:
        ConstLookup() = default;
        ConstLookup(const ConstLookup&) = default;
        ConstLookup& operator=(const ConstLookup& other) {
            if (this == &other) return *this;
            [[maybe_unused]] const auto previous = retained_;
            // MappedReference assignment writes a float; reconstruct its owner instead.
            if (other.retained_) retained_.emplace(*other.retained_);
            else retained_.reset();
            value_ = other.value_;
            return *this;
        }
        explicit operator bool() const noexcept { return value_ != nullptr; }
        const float& operator*() const {
            return *nicxlive::core::d_runtime::ngRequireObject(value_);
        }
        // A raw borrow is valid only while this actual entry owner remains alive.
        const float* get() const noexcept { return value_; }
    };
    bool isNull() const noexcept { return storage_.isNull(); }
    std::size_t size() const noexcept { return storage_.size(); }
    bool empty() const noexcept { return size() == 0; }
    Storage::MappedReference operator[](std::uint64_t key) {
        return storage_[key];
    }
    ConstLookup find(std::uint64_t key) const {
        auto storage = storage_;
        if (storage.find(key) == storage.end()) return {};
        // Pure UUID hash/equality cannot reenter or remove the already-present key.
        // This real existing-cell handle retains the Entry without retaining the table.
        return ConstLookup(storage[key]);
    }
    void remove(std::uint64_t key) { storage_.erase(key); }
    void clear() { storage_.clear(); }
    void reset() noexcept { storage_ = Storage{}; }
    const Storage& entries() const { return storage_; }
private:
    Storage storage_;
};

struct ExDepthInfluenceRule {
    std::uint32_t maxInfluences = 4;
    float radiusScale = 1.0f;
    float minimumRadius = 1.0f;
    compat::RetainedString falloff = compat::RetainedString::fromStaticLiteral("gaussian");
    DepthUuidFloatMap multipliersByBoneUuid;
    template<class Serializer> void serialize(Serializer& serializer) const;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data);
};

struct ExDepthBoneSourceSettings {
    std::uint64_t boneUuid = 0;
    float weight = 1.0f;
    float depthOffset = 0.0f;
    float depthScale = 1.0f;
    float rotation = 0.0f;
    template<class Serializer> void serialize(Serializer& serializer) const;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data);
};

struct ExDepthRigBinding {
    std::uint64_t targetUuid = 0;
    ExDepthTargetKind targetKind = ExDepthTargetKind::Grid;
    compat::Slice<std::uint64_t> sourceBoneUuids;
    compat::Slice<ExDepthBoneSourceSettings> sourceSettings;
    ExDepthInfluenceRule influenceRule;
    std::ptrdiff_t findSourceSettingIndex(std::uint64_t boneUuid) const;
    ExDepthBoneSourceSettings sourceSetting(std::uint64_t boneUuid) const;
    void normalizeSourceSettings();
    void setSourceSetting(ExDepthBoneSourceSettings setting);
    static void setSourceSettingNullable(ExDepthRigBinding* binding, ExDepthBoneSourceSettings setting);
    template<class Serializer> void serialize(Serializer& serializer) const;
    nicxlive::core::serde::ReadError deserializeFromFghj(nicxlive::core::serde::TypedJsonView data);
};

namespace detail {
template<class T> compat::Slice<T> newDepthRigArray(std::size_t count, T initial) {
    if (!count) return {};
    try {
        return compat::Slice<T>(std::vector<T>(count, initial));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    }
}

inline std::uint64_t depthRigMapKey(std::string_view text) {
    // Fghj's to!ulong resolves to mir.conv, including leading plus support.
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    if (text.empty())
        nicxlive::core::serde::detail::throwMirFromStringFailure<std::uint64_t>();
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{})
        nicxlive::core::serde::detail::throwMirFromStringFailure<std::uint64_t>();
    if (parsed.ptr != text.data() + text.size())
        nicxlive::core::serde::detail::throwMirFromStringFailure<std::uint64_t>(true);
    return value;
}
} // namespace detail

template<class Serializer>
void ExDepthInfluenceRule::serialize(Serializer& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putKey("maxInfluences"); serializer.serializeValue(maxInfluences);
    serializer.putKey("radiusScale"); serializer.serializeValue(radiusScale);
    serializer.putKey("minimumRadius"); serializer.serializeValue(minimumRadius);
    serializer.putKey("falloff");
    {
        const auto currentFalloff = falloff;
        serializer.serializeValue(currentFalloff.view());
    }
    if (!multipliersByBoneUuid.empty()) {
        serializer.putKey("multipliersByBoneUuid");
        const auto currentMultipliers = multipliersByBoneUuid;
        const auto mapState = serializer.structBegin();
        for (const auto [uuid, value] : currentMultipliers.entries()) {
            char buffer[40];
            const auto key = std::to_chars(buffer, buffer + sizeof(buffer), uuid);
            serializer.putEscapedKey(std::string_view(buffer, key.ptr - buffer));
            // Fghj AA serialization invokes its free numeric serializer,
            // bypassing InochiSerializer's scalar nonfinite correction.
            if (std::isfinite(value)) serializer.serializeValue(value);
            else if (std::isnan(value))
                serializer.putValue(std::signbit(value) ? std::string_view("\"-nan\"") : std::string_view("\"nan\""));
            else serializer.putValue(value > 0 ? std::string_view("\"inf\"") : std::string_view("\"-inf\""));
        }
        serializer.structEnd(mapState);
    }
    serializer.structEnd(state);
}

inline nicxlive::core::serde::ReadError ExDepthInfluenceRule::deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) {
    using namespace nicxlive::core::serde;
    const auto read = [&data](const char* key, auto& value) -> ReadError {
        const auto entry = data.findFirst(key);
        if (entry.isMissing()) return {};
        return deserializeValue(entry, value);
    };
    if (auto error = read("maxInfluences", maxInfluences)) return error;
    if (auto error = read("radiusScale", radiusScale)) return error;
    if (auto error = read("minimumRadius", minimumRadius)) return error;
    if (auto error = read("falloff", falloff)) return error;
    const auto map = data.findFirst("multipliersByBoneUuid");
    if (!map.isMissing()) {
        if (map.kind() == JsonKind::Null) multipliersByBoneUuid.reset();
        else {
            if (map.kind() != JsonKind::Object) return unexpectedJsonKind(map);
            for (const auto [key, entry] : map.byKeyValue()) {
                float value = std::numeric_limits<float>::quiet_NaN();
                if (auto error = deserializeValue(entry, value)) return error;
                multipliersByBoneUuid[detail::depthRigMapKey(key.view())] = value;
            }
        }
    }
    if (maxInfluences == 0) maxInfluences = 1;
    return {};
}

template<class Serializer>
void ExDepthBoneSourceSettings::serialize(Serializer& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putKey("bone"); serializer.serializeValue(boneUuid);
    serializer.putKey("weight"); serializer.serializeValue(weight);
    serializer.putKey("depthOffset"); serializer.serializeValue(depthOffset);
    serializer.putKey("depthScale"); serializer.serializeValue(depthScale);
    serializer.putKey("rotation");
    const auto normalizedRotation = ngNormalizeDepthBoneSourceRotation(rotation);
    serializer.serializeValue(normalizedRotation);
    serializer.structEnd(state);
}

inline nicxlive::core::serde::ReadError ExDepthBoneSourceSettings::deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) {
    using namespace nicxlive::core::serde;
    const auto read = [&data](const char* key, auto& value) -> ReadError {
        const auto entry = data.findFirst(key);
        if (entry.isMissing()) return {};
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, std::uint64_t>) {
            // Use the same ulong conversion as the other depth-rig readers.
            if (entry.kind() != JsonKind::Number) return unexpectedJsonKind(entry);
            value = detail::depthRigMapKey(entry.numberToken());
            return {};
        } else return deserializeValue(entry, value);
    };
    if (auto error = read("bone", boneUuid)) return error;
    if (auto error = read("weight", weight)) return error;
    if (auto error = read("depthOffset", depthOffset)) return error;
    if (auto error = read("depthScale", depthScale)) return error;
    if (auto error = read("rotation", rotation)) return error;
    rotation = ngNormalizeDepthBoneSourceRotation(rotation);
    return {};
}

inline std::ptrdiff_t ExDepthRigBinding::findSourceSettingIndex(std::uint64_t boneUuid) const {
    for (std::size_t i = 0; i < sourceSettings.size(); ++i)
        if (sourceSettings[i].boneUuid == boneUuid) return static_cast<std::ptrdiff_t>(i);
    return -1;
}

inline ExDepthBoneSourceSettings ExDepthRigBinding::sourceSetting(std::uint64_t boneUuid) const {
    const auto index = findSourceSettingIndex(boneUuid);
    if (index >= 0) return sourceSettings[static_cast<std::size_t>(index)];
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = boneUuid;
    return setting;
}

inline void ExDepthRigBinding::normalizeSourceSettings() {
    compat::Slice<ExDepthBoneSourceSettings> normalized;
    const auto currentSourceBoneUuids = sourceBoneUuids;
    for (auto uuid : currentSourceBoneUuids) {
        auto setting = sourceSetting(uuid);
        setting.rotation = ngNormalizeDepthBoneSourceRotation(setting.rotation);
        normalized.append(setting);
    }
    sourceSettings = normalized;
}

inline void ExDepthRigBinding::setSourceSetting(ExDepthBoneSourceSettings setting) {
    setSourceSettingNullable(this, setting);
}

inline void ExDepthRigBinding::setSourceSettingNullable(ExDepthRigBinding* binding,
        ExDepthBoneSourceSettings setting) {
    setting.rotation = ngNormalizeDepthBoneSourceRotation(setting.rotation);
    auto& receiver = *compat::ngRequireObject(binding);
    bool present = false;
    for (auto uuid : receiver.sourceBoneUuids) if (uuid == setting.boneUuid) { present = true; break; }
    if (!present) receiver.sourceBoneUuids.append(setting.boneUuid);
    receiver.normalizeSourceSettings();
    const auto index = receiver.findSourceSettingIndex(setting.boneUuid);
    if (index >= 0) receiver.sourceSettings[static_cast<std::size_t>(index)] = setting;
}

template<class Serializer>
void ExDepthRigBinding::serialize(Serializer& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putKey("target"); serializer.serializeValue(targetUuid);
    serializer.putKey("targetKind"); serializer.putValue(ngDepthTargetKindName(targetKind));
    serializer.putKey("sourceBoneUuids");
    {
        const auto currentSourceBoneUuids = sourceBoneUuids;
        serializer.serializeValue(currentSourceBoneUuids);
    }
    if (!sourceSettings.empty()) {
        serializer.putKey("sourceSettings");
        const auto currentSourceSettings = sourceSettings;
        serializer.serializeValue(currentSourceSettings);
    }
    serializer.putKey("influenceRule"); influenceRule.serialize(serializer);
    serializer.structEnd(state);
}

inline nicxlive::core::serde::ReadError ExDepthRigBinding::deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) {
    using namespace nicxlive::core::serde;
    const auto target = data.findFirst("target");
    if (!target.isMissing()) {
        if (target.kind() != JsonKind::Number) return unexpectedJsonKind(target);
        targetUuid = detail::depthRigMapKey(target.numberToken());
    }
    const auto kind = data.findFirst("targetKind");
    if (!kind.isMissing()) {
        compat::RetainedString text;
        if (auto error = deserializeValue(kind, text)) return error;
        try { targetKind = ngParseDepthTargetKind(text.view()); }
        catch (const compat::ThrowableException& error) {
            return ReadError(std::allocate_shared<ReadError::Object>(
                nicxlive::core::d_runtime::SourceAllocator<ReadError::Object>{}, error.msg,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 206));
        }
    }
    const auto readArray = []<class T, class Read>(TypedJsonView entry, compat::Slice<T>& values,
            Read read) -> ReadError {
        if (entry.kind() == JsonKind::Null) { values.clear(); return {}; }
        if (entry.kind() != JsonKind::Array) return unexpectedJsonKind(entry);
        auto elements = entry.byElement();
        std::size_t count = 0;
        for (auto remaining = elements.save(); !remaining.empty(); remaining.popFront()) ++count;
        values = detail::newDepthRigArray<T>(count, T{});
        auto currentValues = values;
        for (auto& value : currentValues) {
            if (auto error = read(elements.front(), value)) return error;
            elements.popFront();
        }
        return {};
    };
    const auto uuids = data.findFirst("sourceBoneUuids");
    if (!uuids.isMissing())
        if (auto error = readArray(uuids, sourceBoneUuids, [](TypedJsonView entry, std::uint64_t& uuid) -> ReadError {
                if (entry.kind() != JsonKind::Number) return unexpectedJsonKind(entry);
                uuid = detail::depthRigMapKey(entry.numberToken()); return {};
            })) return error;
    const auto settings = data.findFirst("sourceSettings");
    if (!settings.isMissing())
        if (auto error = readArray(settings, sourceSettings, [](TypedJsonView entry,
                ExDepthBoneSourceSettings& setting) { return setting.deserializeFromFghj(entry); })) return error;
    const auto rule = data.findFirst("influenceRule");
    if (!rule.isMissing())
        if (auto error = influenceRule.deserializeFromFghj(rule)) return error;
    normalizeSourceSettings();
    return {};
}

} // namespace nijigenerate::ext::nodes

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_RIG_VALUE_SOURCE_TYPE(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exdepthbone." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exdepthbone." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_DEPTH_RIG_VALUE_SOURCE_TYPE(ExDepthInfluenceRule)
NG_DEPTH_RIG_VALUE_SOURCE_TYPE(ExDepthBoneSourceSettings)
NG_DEPTH_RIG_VALUE_SOURCE_TYPE(ExDepthRigBinding)
#undef NG_DEPTH_RIG_VALUE_SOURCE_TYPE
}
