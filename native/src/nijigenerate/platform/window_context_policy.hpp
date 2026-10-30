#pragma once
#include <string_view>

struct ImGuiIO;

namespace nijigenerate::platform::detail {
void configureWindowContextHost(ImGuiIO& io);
bool windowConfigFileExists(std::string_view path);
}
