#pragma once
#include <imgui.h>
namespace nijigenerate::core {
ImVec4 ngUiAccentColor(float alpha = 1.f);
void ngInitStyling();
void ngPushDarkColorScheme();
void ngPushLightColorScheme();
void ngPopColorScheme();
bool ngGetDarkMode();
}
