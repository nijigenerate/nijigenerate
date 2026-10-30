#include "file_helpers.hpp"
#include <imgui.h>

namespace nijigenerate::commands::puppet::file::detail {

double ngRealtimeNow() { return ImGui::GetTime(); }

} // namespace nijigenerate::commands::puppet::file::detail
