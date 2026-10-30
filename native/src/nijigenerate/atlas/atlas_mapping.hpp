#pragma once

#include <nodes/part.hpp>
#include <common/resource_key_map.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <utility>

namespace nijigenerate::atlas::atlas {

// Actual retained descriptor for this source's rect[uint] field.
class AtlasMappings {
    using Value = nicxlive::core::nodes::Rect;
    struct KeyHash {
        std::size_t operator()(std::uint32_t key) const noexcept { return static_cast<std::size_t>(key); }
    };
    using Storage = nicxlive::core::common::ResourceKeyMap<std::uint32_t, Value,
        KeyHash, std::equal_to<std::uint32_t>>;
    Storage storage_;
public:
    AtlasMappings() noexcept = default;
    AtlasMappings(const AtlasMappings&) noexcept = default;
    AtlasMappings& operator=(const AtlasMappings&) noexcept = default;
    AtlasMappings(AtlasMappings&& other) noexcept : storage_(other.storage_) {}
    AtlasMappings& operator=(AtlasMappings&& other) noexcept {
        storage_ = other.storage_;
        return *this;
    }
    bool isNull() const noexcept { return storage_.isNull(); }
    bool empty() const noexcept { return storage_.empty(); }
    std::size_t size() const noexcept { return storage_.size(); }
    Value* find(std::uint32_t key) {
        const auto found = storage_.find(key);
        return found == storage_.end() ? nullptr : std::addressof(found->second);
    }
    const Value* find(std::uint32_t key) const {
        const auto found = storage_.find(key);
        return found == storage_.end() ? nullptr : std::addressof(found->second);
    }
    bool contains(std::uint32_t key) const { return find(key) != nullptr; }
    void set(std::uint32_t key, Value value) {
        storage_[key] = std::move(value);
    }
    bool remove(std::uint32_t key) { return storage_.erase(key) != 0; }
    void clear() { storage_.clear(); }
    void reset() noexcept { storage_ = Storage{}; }
};

} // namespace nijigenerate::atlas::atlas
