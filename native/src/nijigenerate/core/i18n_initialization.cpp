#include "i18n.hpp"
#include "i18n_marker.hpp"
#include "path.hpp"
#include "nijigenerate/compat/phobos_index_sort.hpp"
#include <vector>

namespace nijigenerate::core::i18n {
void ngLocaleInit() {
    detail::incLocaleScan(path::ngGetAppLocalePath());
    detail::incLocaleScan(platform::localeExecutableDirectory());
    const auto platformPath = platform::localePlatformScanDirectory();
    if (!platformPath.isNull()) detail::incLocaleScan(platformPath);
    const auto extraPath = path::ngGetAppLocalePathExtra();
    if (!extraPath.isNull()) detail::incLocaleScan(extraPath);

    auto files = detail::localeFiles;
    if (files.size() > 1) {
        std::vector<std::size_t> order;
        detail::allocateLocaleStorage([&] { order.reserve(files.size()); });
        for (std::size_t i = 0; i < files.size(); ++i) order.push_back(i);
        const auto original = detail::allocateLocaleStorage([&] {
            return std::vector<TLEntry>(files.begin(), files.end());
        });
        const auto applyOrder = [&] {
            for (std::size_t i = 0; i < files.size(); ++i) files[i] = original[order[i]];
        };
        // Original TLEntry has four string descriptors and one C pointer.
        try {
            compat::phobos_index_sort::sort<9 * sizeof(void*)>(order,
                [&](std::size_t a, std::size_t b) { return ngCompareEntries(original[a], original[b]); },
                "Failed to sort range of type TLEntry[]");
        } catch (...) {
            // D sorts the retained array in place; a failure does not roll its writes back.
            applyOrder();
            throw;
        }
        applyOrder();
    }
    ngMarkDups(std::span<TLEntry>(files.data(), files.size()));
}

void ngLocaleInitFromSettings() {
    ngLocaleInit();
    const auto key = compat::RetainedString::fromStaticLiteral("lang");
    if (settings::incSettingsCanGet(key)) {
        const auto language = settings::incSettingsGet<settings::SettingsString>(key);
        const auto entry = ngLocaleGetEntryFor(language.view());
        if (entry) translation::ngLoadLanguage(entry->file);
    }
}
}
