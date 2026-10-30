#pragma once
#include <cstddef>
#include <string_view>
namespace nijigenerate::platform {
struct CatalogEntryNameRange {
    std::size_t first;
    std::size_t last;
};
CatalogEntryNameRange localeCodeRangeFromMoEntry(std::string_view bytes);
bool localeMoFilenameMatches(std::string_view filename);
} // namespace nijigenerate::platform
