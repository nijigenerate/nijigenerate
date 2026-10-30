#pragma once
#include <filesystem>

namespace nijigenerate::platform {
void ngRenameReplacingExisting(const std::filesystem::path& source, const std::filesystem::path& destination);
}
