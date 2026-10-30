#include "nijigenerate/platform/locale_catalog_entry_name.hpp"

namespace nijigenerate::platform {
// Input is a real shallow DirEntry.name, with a filename and no trailing separator.
CatalogEntryNameRange localeCodeRangeFromMoEntry(std::string_view bytes) {
    std::size_t first = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '/'
#if defined(_WIN32)
            || bytes[i] == '\\' || bytes[i] == ':'
#endif
        ) first = i + 1;
    }
    // Phobos stripExtension leaves a leading dot alone, including a bare .mo.
    auto last = bytes.size();
    for (auto i = last; i > first; ) {
        --i;
        if (bytes[i] == '.' && i > first) { last = i; break; }
    }
    return {first, last};
}

bool localeMoFilenameMatches(std::string_view filename) {
    if (filename.size() < 3) return false;
    const auto end = filename.size();
    const auto equalLetter = [](char value, char letter) {
#if defined(_WIN32)
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value + ('a' - 'A'));
#endif
        return value == letter;
    };
    return filename[end - 3] == '.' && equalLetter(filename[end - 2], 'm')
        && equalLetter(filename[end - 1], 'o');
}
} // namespace nijigenerate::platform
