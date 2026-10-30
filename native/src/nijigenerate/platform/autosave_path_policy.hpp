#pragma once
#include <string>
#include <string_view>

namespace nijigenerate::platform {
// The returned basename borrows the supplied immutable byte view.
std::string_view ngAutosaveBaseName(std::string_view path) noexcept;
std::string_view ngAutosaveStripExtension(std::string_view path) noexcept;
// basename must be the result of ngAutosaveBaseName, not an arbitrary path.
std::string ngAutosaveBuildDirectory(std::string_view config, std::string_view basename);
}
