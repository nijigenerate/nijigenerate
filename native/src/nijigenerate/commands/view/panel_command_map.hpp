#pragma once
#include "panel_key.hpp"
#include "nijigenerate/commands/command.hpp"
#include <common/resource_key_map.hpp>
#include "nijigenerate/compat/range_error.hpp"
#include <functional>
#include <memory>
#include <utility>

namespace nijigenerate::commands::view::panel {

// This registry retains the source's distinct PanelKey domain and AA value copies.
class PanelCommandMap {
    struct Hash {
        std::size_t operator()(const PanelKey& key) const noexcept { return key.toHash(); }
    };
    using Storage = nicxlive::core::common::ResourceKeyMap<PanelKey, std::shared_ptr<Command>,
        Hash, std::equal_to<PanelKey>>;
    Storage storage_;
public:
    using const_iterator = Storage::const_iterator;
    const_iterator begin() const { return storage_.begin(); }
    const_iterator end() const { return storage_.end(); }
    bool isNull() const noexcept { return storage_.isNull(); }
    std::size_t size() const noexcept { return storage_.size(); }
    bool empty() const noexcept { return size() == 0; }
    std::shared_ptr<std::shared_ptr<Command>> find(const PanelKey& key) {
        return storage_.find(key).retainValue();
    }
    std::shared_ptr<const std::shared_ptr<Command>> find(const PanelKey& key) const {
        return storage_.find(key).retainValue();
    }
    std::shared_ptr<Command> get(const PanelKey& key) const {
        const auto found = find(key);
        if (!found) throw compat::RangeError();
        return *found;
    }
    void set(PanelKey key, std::shared_ptr<Command> command) {
        storage_[key] = std::move(command);
    }
    bool remove(const PanelKey& key) {
        return storage_.erase(key) != 0;
    }
    void clear() { storage_.clear(); }
    void reset() noexcept { storage_ = {}; }
};

inline thread_local PanelCommandMap togglePanelCommands;
} // namespace nijigenerate::commands::view::panel
