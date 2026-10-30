#pragma once

#include <nijigenerate/commands/automesh/dynamic_key.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/commands/detail/map_selection.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::commands::automesh::dynamic {

// Retain the module AA descriptor and the canonical key/command identities.
// Hash/equality remain the real AutoMeshKey methods, with their own body gates.
class AutoMeshApplyCommandMap {
    struct Hash {
        std::size_t operator()(const AutoMeshKey& key) const noexcept { return key.toHash(); }
    };
    struct Equal {
        bool operator()(const AutoMeshKey& left, const AutoMeshKey& right) const noexcept {
            return left.opEquals(right);
        }
    };
    using Storage = CommandMapStorage<AutoMeshKey, std::shared_ptr<Command>, Hash, Equal>;

public:
    using const_iterator = Storage::const_iterator;

    bool isNull() const noexcept { return storage_.isNull(); }
    std::size_t size() const noexcept { return storage_.size(); }
    bool empty() const noexcept { return size() == 0; }
    const_iterator begin() const { return read().begin(); }
    const_iterator end() const { return read().end(); }
    const_iterator find(const AutoMeshKey& key) const { return read().find(key); }

    std::shared_ptr<Command>& operator[](const AutoMeshKey& key) {
        return storage_[key];
    }

private:
    const Storage& read() const {
        return storage_;
    }
    Storage storage_;
};

extern thread_local AutoMeshApplyCommandMap autoMeshApplyCommands;

} // namespace nijigenerate::commands::automesh::dynamic

namespace nijigenerate::commands::detail {

// The original AA has this exact key. Extend the canonical key extractor for
// the actual retained map type, without substituting a second SelectorMap.
template<> struct AssociativeMapKey<automesh::dynamic::AutoMeshApplyCommandMap> {
    using type = automesh::dynamic::AutoMeshKey;
};

} // namespace nijigenerate::commands::detail
