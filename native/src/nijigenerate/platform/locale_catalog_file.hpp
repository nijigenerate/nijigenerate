#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include <common/shared_array.hpp>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace nijigenerate::platform {
using LocaleCatalogBytes = nicxlive::core::common::SharedArray<std::uint8_t>;
LocaleCatalogBytes ngReadLocaleCatalogBytesRetained(compat::RetainedString filename);

class CatalogFileError : public std::runtime_error {
public:
    explicit CatalogFileError(std::uint32_t value)
        : std::runtime_error("Catalog file operation failed"), nativeCode(value) {}
    std::uint32_t nativeCode;
};
std::vector<std::uint8_t> readLocaleCatalogBytes(std::string_view filename, bool nullFilename);
}
