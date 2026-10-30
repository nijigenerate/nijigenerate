#pragma once
#include "i18n_state.hpp"
#include <string_view>

namespace nijigenerate::core::i18n {
class LocaleEntryHandle {
public:
    LocaleEntryHandle() = default;
    TLEntry* get() const { return backing_.data(); }
    explicit operator bool() const { return get() != nullptr; }
    TLEntry* operator->() const { return get(); }
    TLEntry& operator*() const { return *get(); }
private:
    explicit LocaleEntryHandle(compat::Slice<TLEntry> backing) : backing_(std::move(backing)) {}
    // Constness of the pointer descriptor does not make its pointee immutable.
    mutable compat::Slice<TLEntry> backing_;
    friend LocaleEntryHandle ngLocaleGetEntryFor(std::string_view code);
};

inline LocaleEntryHandle ngLocaleGetEntryFor(std::string_view code) {
    for (std::size_t i = 0; i < detail::localeFiles.size(); ++i)
        if (detail::localeFiles[i].code.view() == code)
            return LocaleEntryHandle(detail::localeFiles.slice(i, i + 1));
    return {};
}
} // namespace nijigenerate::core::i18n
