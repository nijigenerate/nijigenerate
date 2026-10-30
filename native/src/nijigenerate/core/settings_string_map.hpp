#pragma once
#include "settings_value.hpp"
#include <initializer_list>
#include <utility>

namespace nijigenerate::core::settings {
// Typed string AA descriptors retain both key and value strings. Null copies
// allocate independently; initialized copies share replacements and insertions.
class SettingsStringMap {
    using Members = nicxlive::core::common::ResourceKeyMap<SettingsString, SettingsString,
        detail::SettingsJsonKeyHash, detail::SettingsJsonKeyEqual>;
public:
    using Entry = std::pair<SettingsString, SettingsString>;
    SettingsStringMap() noexcept = default;
    SettingsStringMap(std::initializer_list<Entry> entries) {
        for (const auto& [key, value] : entries) (*this)[key] = value;
    }
    bool isNull() const noexcept { return members_.isNull(); }
    std::size_t size() const noexcept { return members_.size(); }
    bool empty() const noexcept { return size() == 0; }
    const void* identity() const noexcept { return members_.identity(); }
    SettingsString& operator[](SettingsString key) {
        return members_[key];
    }
    const SettingsString* find(std::string_view key) const noexcept {
        // The native view bridge must not allocate a new owning lookup string.
        for (const auto& [name, value] : members_) if (name.view() == key) return &value;
        return nullptr;
    }
    Members::const_iterator begin() const noexcept { return members_.begin(); }
    Members::const_iterator end() const noexcept { return members_.end(); }
private:
    Members members_;
};
} // namespace nijigenerate::core::settings
