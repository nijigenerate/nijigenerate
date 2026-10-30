#pragma once
#include "i18n_entry.hpp"
#include <span>

namespace nijigenerate::core::i18n {
inline void ngMarkDups(std::span<TLEntry> entries) {
    if (entries.size() <= 1) return;
    auto* previous = &entries[0];
    bool previousIsDuplicate = false;
    auto rename = [](TLEntry& entry) {
        entry.humanName = detail::allocateLocaleStorage([&] {
            return compat::RetainedString(std::string(entry.humanName.view()) +
                " (" + std::string(entry.code.view()) + ")");
        });
        detail::refreshLocaleNameCString(entry);
    };
    for (std::size_t i = 1; i < entries.size(); ++i) {
        auto& entry = entries[i];
        const bool entryIsDuplicate = entry.humanName == previous->humanName;
        if (previousIsDuplicate || entryIsDuplicate) rename(*previous);
        previousIsDuplicate = entryIsDuplicate;
        previous = &entry;
    }
    if (previousIsDuplicate) rename(*previous);
}
} // namespace nijigenerate::core::i18n
