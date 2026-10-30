#pragma once

#include <array>
#include <span>

namespace nijigenerate::platform::tinyfiledialogs {
// Explicit C++ views replace the D slice ABI; no prebuilt D library is linked.
struct TFD_Filter {
    std::span<const char* const> patterns;
    const char* description = nullptr;
};
extern const char tinyfd_version[8];
extern const char tinyfd_needs[];
extern bool tinyfd_verbose;
extern bool tinyfd_silent;
extern bool tinyfd_forceConsole;
extern char tinyfd_response[1024];

void tinyfd_beep();
int tinyfd_notifyPopup(const char* title, const char* message, const char* iconType);
int tinyfd_messageBox(const char* title, const char* message, const char* dialogType,
    const char* iconType, int defaultButton);
const char* tinyfd_inputBox(const char* title, const char* message, const char* defaultInput);
const char* tinyfd_saveFileDialog(const char* title, const char* defaultPathAndFile,
    std::span<const TFD_Filter> filters);
const char* tinyfd_openFileDialog(const char* title, const char* defaultPathAndFile,
    std::span<const TFD_Filter> filters, bool allowMultipleSelects);
const char* tinyfd_selectFolderDialog(const char* title, const char* defaultPath);
const char* tinyfd_colorChooser(const char* title, const char* defaultHexRGB,
    const std::array<unsigned char, 3>& defaultRGB, std::array<unsigned char, 3>& resultRGB);
} // namespace nijigenerate::platform::tinyfiledialogs
