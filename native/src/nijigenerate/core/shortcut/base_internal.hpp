#pragma once

#include "base.hpp"
#include <nijigenerate/commands/command.hpp>
#include <common/object_key_policy.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>

namespace nijigenerate::core::shortcut::detail {
using CommandRef = std::shared_ptr<commands::Command>;
using CommandPolicy = nicxlive::core::common::ObjectClassKeyPolicy<CommandRef>;
using ShortcutRegistry = nicxlive::core::common::ResourceKeyMap<CommandRef, ActionEntry,
    CommandPolicy::Hash, CommandPolicy::Equal>;
struct ShortcutStringHash {
    std::size_t operator()(const compat::RetainedString& value) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(value.view());
    }
};
struct ShortcutStringEqual {
    bool operator()(const compat::RetainedString& left, const compat::RetainedString& right) const noexcept {
        return left.view() == right.view();
    }
};
using ShortcutIdMap = nicxlive::core::common::ResourceKeyMap<compat::RetainedString, CommandRef,
    ShortcutStringHash, ShortcutStringEqual>;
extern thread_local ShortcutRegistry shortcutEntries;
}
