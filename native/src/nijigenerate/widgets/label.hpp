#pragma once
#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::widgets {
void ngText(compat::RetainedStringSlice text);
void ngTextColored(ImVec4 color, compat::RetainedStringSlice text);
void ngTextDisabled(compat::RetainedStringSlice text);
void ngTextShadowed(compat::RetainedStringSlice text);
void ngTextBordered(compat::RetainedStringSlice text, ImVec4 borderColor = ImVec4(0, 0, 0, 1));
void ngTextLabel(compat::RetainedStringSlice text);
void ngTextWrapped(compat::RetainedStringSlice text);
bool ngTextLinkWithIcon(compat::RetainedStringSlice icon, compat::RetainedStringSlice text);
bool ngTextLink(compat::RetainedStringSlice text, ImVec4 hoverColor = ImVec4(.313f, .521f, .737f, 1),
                ImVec4 clickedColor = ImVec4(.132f, .335f, .523f, 1),
                ImVec4 baseColor = ImVec4(.186f, .457f, .708f, 1));
// Native compatibility views borrow caller storage for the complete operation.
void ngText(std::string_view text);
void ngTextColored(ImVec4 color, std::string_view text);
void ngTextDisabled(std::string_view text);
void ngTextShadowed(std::string_view text);
void ngTextBordered(std::string_view text, ImVec4 borderColor = ImVec4(0, 0, 0, 1));
void ngTextLabel(std::string_view text);
void ngTextWrapped(std::string_view text);
bool ngTextLinkWithIcon(std::string_view icon, std::string_view text);
bool ngTextLink(std::string_view text, ImVec4 hoverColor = ImVec4(.313f, .521f, .737f, 1),
                ImVec4 clickedColor = ImVec4(.132f, .335f, .523f, 1),
                ImVec4 baseColor = ImVec4(.186f, .457f, .708f, 1));
namespace label {
using ::nijigenerate::widgets::ngText;
using ::nijigenerate::widgets::ngTextColored;
using ::nijigenerate::widgets::ngTextDisabled;
using ::nijigenerate::widgets::ngTextShadowed;
using ::nijigenerate::widgets::ngTextBordered;
using ::nijigenerate::widgets::ngTextLabel;
using ::nijigenerate::widgets::ngTextWrapped;
using ::nijigenerate::widgets::ngTextLinkWithIcon;
using ::nijigenerate::widgets::ngTextLink;
}
}
