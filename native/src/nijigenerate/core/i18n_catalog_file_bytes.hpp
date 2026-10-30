#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/platform/locale_catalog_file.hpp"
#include <utility>

namespace nijigenerate::core::i18n::detail {
using CatalogFileError = platform::CatalogFileError;
inline platform::LocaleCatalogBytes readLocaleCatalogBytesRetained(compat::RetainedString filename) {
    return platform::ngReadLocaleCatalogBytesRetained(std::move(filename));
}
inline std::vector<std::uint8_t> readLocaleCatalogBytes(const compat::RetainedString& filename) {
    return platform::readLocaleCatalogBytes(filename.view(), filename.isNull());
}
}
