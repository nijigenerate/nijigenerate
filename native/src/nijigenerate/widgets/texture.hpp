#pragma once
#include <imgui.h>
#include <memory>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>
namespace nicxlive::core { class Texture; }
namespace nijigenerate::widgets {
void ngTextureSlot(compat::RetainedStringSlice text, std::shared_ptr<nicxlive::core::Texture> texture,
                   ImVec2 size = ImVec2(92, 92), float gridSize = 32, ImGuiWindowFlags flags = 0);
void ngTextureSlotUntitled(compat::RetainedStringSlice name, std::shared_ptr<nicxlive::core::Texture> texture,
                           ImVec2 size = ImVec2(92, 92), float gridSize = 32,
                           ImGuiWindowFlags flags = 0, bool selected = false);
// Legacy byte views own their actual input bytes before entering these primaries.
void ngTextureSlot(std::string_view text, std::shared_ptr<nicxlive::core::Texture> texture,
                   ImVec2 size = ImVec2(92, 92), float gridSize = 32, ImGuiWindowFlags flags = 0);
void ngTextureSlotUntitled(std::string_view name, std::shared_ptr<nicxlive::core::Texture> texture,
                           ImVec2 size = ImVec2(92, 92), float gridSize = 32,
                           ImGuiWindowFlags flags = 0, bool selected = false);
}
namespace nijigenerate::widgets::texture {
using ::nijigenerate::widgets::ngTextureSlot;
using ::nijigenerate::widgets::ngTextureSlotUntitled;
}
