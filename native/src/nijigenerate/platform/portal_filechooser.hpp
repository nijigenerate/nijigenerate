#pragma once
#include "tinyfiledialogs.hpp"
#include <optional>
#include <span>
#include <string>
#include <string_view>
struct SDL_Window;

namespace nijigenerate::platform::portal {
// Corresponds to dportals.dpInit; Linux window initialization must call it.
void ngInitializePortal();
// Corresponds to dportals.dpUpdate/ddbus.tick on the current connection.
bool ngUpdatePortal();
std::optional<std::string> ngChoosePortalFile(SDL_Window* window, std::string_view title,
    std::span<const tinyfiledialogs::TFD_Filter> filters, bool save,
    bool multiple = false, bool directory = false);
}
