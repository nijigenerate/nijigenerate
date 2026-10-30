// The catalog and translation paths share the actual Phobos read provider.
#include "nijigenerate/platform/locale_catalog_file.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <compat/phobos_file_read.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::platform {

LocaleCatalogBytes ngReadLocaleCatalogBytesRetained(compat::RetainedString filename) {
    return nicxlive::core::d_runtime::ngReadBinaryFile(std::move(filename));
}

std::vector<std::uint8_t> readLocaleCatalogBytes(std::string_view filename, bool nullFilename) {
    // Native compatibility only. Authored callers retain the actual byte-array
    // descriptor returned by the primary above, without this native vector copy.
    const auto name = nullFilename ? compat::RetainedString{} :
        compat::RetainedString::fromBorrowedView(filename);
    LocaleCatalogBytes bytes;
    try {
        bytes = ngReadLocaleCatalogBytesRetained(name);
    } catch (const nicxlive::core::d_runtime::FileException& error) {
        throw CatalogFileError(error.errorNumber);
    }
    if (bytes.empty()) return {};
    try {
        return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

} // namespace nijigenerate::platform
