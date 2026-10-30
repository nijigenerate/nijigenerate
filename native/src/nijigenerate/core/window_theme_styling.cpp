#include "nijigenerate/core/window_theme_internal.hpp"
#include "nijigenerate/platform/window_style.hpp"
#include "nijigenerate/widgets/button.hpp"
#include <algorithm>
#include <iterator>
namespace nijigenerate::core {
void ngInitStyling() {
    //style.WindowBorderSize = 0;
    auto& state = detail::windowThemeState();
    auto& style = ImGui::GetStyle();

    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;

    /*
    style.WindowRounding = 4;
    style.ChildRounding = 0;
    style.FrameRounding = 3;
    style.PopupRounding = 6;
    style.ScrollbarRounding = 18;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 6;
    style.TabRounding = 6;

    style.IndentSpacing = 10;
    style.ItemSpacing.y = 3;
    style.FramePadding.y = 4;

    style.GrabMinSize = 13;
    style.ScrollbarSize = 14;
    style.ChildBorderSize = 1;
    */

    // Don't draw the silly roll menu
    style.WindowMenuButtonPosition = ImGuiDir_None;

    if (platform::ngCenterWindowTitle()) {
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    }


    ImGui::StyleColorsDark(&style);
    style.Colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]               = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.16f);
    style.Colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.16f);
    style.Colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.15f, 0.15f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.67f);
    style.Colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]          = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg]              = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]              = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]             = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_Button]                 = ImVec4(0.39f, 0.39f, 0.39f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]          = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]           = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    style.Colors[ImGuiCol_Separator]              = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.29f, 0.29f, 0.29f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive]        = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.40f, 0.40f, 0.40f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.55f, 0.55f, 0.56f, 0.00f);
    style.Colors[ImGuiCol_Tab]                    = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TabHovered]             = ImVec4(0.34f, 0.34f, 0.34f, 0.80f);
    style.Colors[ImGuiCol_TabActive]              = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]           = ImVec4(0.14f, 0.14f, 0.14f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    style.Colors[ImGuiCol_DockingPreview]         = ImVec4(0.62f, 0.68f, 0.75f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_TableRowBg]             = ImVec4(0.310f, 0.310f, 0.310f, 0.267f);
    style.Colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.463f, 0.463f, 0.463f, 0.267f);
    style.Colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight]           = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    std::copy(std::begin(style.Colors), std::end(style.Colors), state.darkColors.begin());

    ImGui::StyleColorsLight(&style);
    // Accent palette based on green and neutral tones
    ImVec4 accentGreen = ngUiAccentColor();                  // primary green accent
    ImVec4 darkGreen = ImVec4(0.18f, 0.23f, 0.18f, 1.00f);   // darker green accent
    ImVec4 lightGreen = ImVec4(0.67f, 0.75f, 0.63f, 1.00f);  // lighter green accent
    ImVec4 black = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);       // near-black text
    ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);       // pure white
    ImVec4 grey = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);        // medium grey
    ImVec4 lightGrey = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);   // light grey

    // Use dark text on light background
    style.Colors[ImGuiCol_Text] = black;

    // Background colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);

    // Window title bar
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

    // Status/toolbar backgrounds and docking
    style.Colors[ImGuiCol_DockingPreview] = lightGrey; // docking preview background
    style.Colors[ImGuiCol_DockingEmptyBg] = white;     // empty docking area background

    // Progress bar colors
    style.Colors[ImGuiCol_PlotHistogram] = accentGreen;
    style.Colors[ImGuiCol_PlotHistogramHovered] = darkGreen;

    // Buttons
    style.Colors[ImGuiCol_Button] = accentGreen;
    style.Colors[ImGuiCol_ButtonHovered] = darkGreen;
    style.Colors[ImGuiCol_ButtonActive] = black;

    // Checkbox checkmark uses green accent
    style.Colors[ImGuiCol_CheckMark] = accentGreen;

    // InputText: white background with grey border
    style.Colors[ImGuiCol_FrameBg] = white;
    style.Colors[ImGuiCol_FrameBgHovered] = grey;
    style.Colors[ImGuiCol_FrameBgActive] = lightGreen;
    style.Colors[ImGuiCol_Border] = grey; // border color set to grey

    // Tabs
    style.Colors[ImGuiCol_Tab] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.97f, 0.975f, 0.97f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

    // List/selection headers
    style.Colors[ImGuiCol_Header] = grey;
    style.Colors[ImGuiCol_HeaderHovered] = lightGreen;
    style.Colors[ImGuiCol_HeaderActive] = darkGreen;

    // Scrollbar background same as window background
    style.Colors[ImGuiCol_ScrollbarBg] = white;

    // Scrollbar colors
    style.Colors[ImGuiCol_ScrollbarGrab] = grey;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = darkGreen;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = black;

    // Slider colors
    style.Colors[ImGuiCol_SliderGrab] = accentGreen;
    style.Colors[ImGuiCol_SliderGrabActive] = darkGreen;

    // Table colors (header/border/rows) matching overall style
    // Header uses light grey like title bars; borders are grey; rows use subtle striping
    style.Colors[ImGuiCol_TableHeaderBg]     = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_TableBorderLight]  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    // Row background uses subtle stripes over white background
    style.Colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.95f, 0.95f, 0.95f, 0.45f);

    // Misc style parameters
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;

    // Spacing and alignments
    style.FramePadding = ImVec2(12.0f, 8.0f);        // padding inside widgets
    style.ItemSpacing = ImVec2(8.0f, 4.0f);          // spacing between items
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);     // spacing inside items
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);      // center button text
    style.DisplaySafeAreaPadding = ImVec2(10.0f, 10.0f); // extra margin around content
    widgets::ngButtonTextColor(ImVec4(1, 1, 1, 1));

    /*
    style.Colors[ImGuiCol_Border] = ImVec4(0.8, 0.8, 0.8, 0.5);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0.05);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.902, 0.902, 0.902, 1);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.86, 0.86, 0.86, 1);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.68, 0.68, 0.68, 1);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.68, 0.68, 0.68, 1);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.64, 0.64, 0.64, 1);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.78, 0.88, 1, 1);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.76, 0.86, 1, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.8, 0.8, 0.8, 1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0, 0, 0, 1);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.98, 0.98, 0.98, 1);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.8, 0.8, 0.8, 1);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.92, 0.92, 0.92, 1);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.88, 0.88, 0.88, 1);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.863, 0.863, 0.863, 1);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.941, 0.941, 0.941, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(0.990, 0.990, 0.990, 1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 1);
    */
    std::copy(std::begin(style.Colors), std::end(style.Colors), state.lightColors.begin());

    const auto& selected = state.isDarkMode ? state.darkColors : state.lightColors;
    std::copy(selected.begin(), selected.end(), std::begin(style.Colors));
}
}
