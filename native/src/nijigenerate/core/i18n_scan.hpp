#pragma once
#include "i18n_catalog_bytes.hpp"
#include "i18n_catalog_file_bytes.hpp"
#include "i18n_catalog_entry_name.hpp"
#include "nijigenerate/platform/locale_scan.hpp"
#include <functional>

namespace nijigenerate::core::i18n::detail {
inline void incLocaleScan(compat::RetainedString path) {
    const auto consumer = allocateLocaleStorage([&] {
        return std::function<void(compat::RetainedString)>([&](compat::RetainedString filename) {
            auto code = localeCodeFromMoEntry(filename);
            if (!cultureCodeValid(code.view())) return;
            const auto bytes = readLocaleCatalogBytesRetained(filename);
            auto name = catalogNameFromBytes({bytes.data(), bytes.size()}, code.view());
            if (name.view() == "<UNKNOWN LANGUAGE>") name = cultureExpression(code.view());
            for (const auto entry : localeFiles) if (entry.code == code) return;
            TLEntry entry{name, nullptr, code, filename, path};
            refreshLocaleNameCString(entry);
            localeFiles.append(entry);
        });
    });
    platform::visitLocaleCatalogEntries(path, consumer);
}
}
