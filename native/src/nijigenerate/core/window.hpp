#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_opengl3.h>
#include <render/common.hpp>

#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/core/window_platform.hpp"
#include "nijigenerate/core/window_theme.hpp"
#include "nijigenerate/platform/window_sdl3_binding.hpp"
#if defined(_WIN32)
#include "nijigenerate/core/window_win32_dpi.hpp"
#endif

#include <cstddef>
#include <cstdint>

namespace nijigenerate::core {

// The native SDK profile uses the actual selected target, not guessed SDL2 DLLs.
using ::nijigenerate::platform::SDL3Binding;
using ::nijigenerate::platform::ngSelectedSDL3Binding;
using ::nijigenerate::platform::ngRequireSDL3Runtime;

extern thread_local bool ngShowStatsForNerds;
extern thread_local bool ngDifferenceAggregationDebugEnabled;
extern thread_local std::size_t ngDifferenceAggregationTargetIndex;
extern thread_local std::size_t ngDifferenceAggregationResolvedIndex;
extern thread_local nicxlive::core::DifferenceEvaluationResult ngDifferenceAggregationResult;
extern thread_local bool ngDifferenceAggregationResultValid;
extern thread_local std::uint64_t ngDifferenceAggregationResultSerial;
nicxlive::core::DifferenceEvaluationResult ngDifferenceAggregationInitialResult();

// Native transport for the source string[] descriptor. ImGui copies only this
// trivially-copyable view; the window state retains its elements while active.
struct WindowFileDropPayload {
    std::size_t length;
    const compat::RetainedStringSlice* data;
};
compat::Slice<compat::RetainedStringSlice> ngRetainFileDropPayload(const WindowFileDropPayload& payload);

bool ngIsTilingWM();
void ngOpenWindow();
void ngCreateContext();
SDL_Window* ngGetWindowPtr();
void ngRefreshWindowTitle();
void ngSetWindowTitle(compat::RetainedStringSlice subtitle);
void ngUpdateWindowTitleTick();
void ngFinalize();
ImGuiID ngGetViewportDockSpace();
void ngSetDarkMode(bool darkMode);
bool ngShouldProcess();
void ngFinishFileDrag();
void ngBeginLoopNoEv();
void ngSetDefaultLayout();
void ngBeginLoop();
void ngEndLoop();
void ngDebugImGuiState(compat::RetainedStringSlice message, int indent = 0);
void ngResetClearColor();
bool ngViewportHasTemporaryBackgroundColor();
void ngViewportGetBackgroundColor(ImVec4& color);
ImVec4 ngViewportGetBackgroundColor();
void ngViewportSetTemporaryBackgroundColor(float red, float green, float blue, float alpha = 1.f);
void ngViewportSetTemporaryBackgroundColor(ImVec4 color);
void ngViewportClearTemporaryBackgroundColor();
bool ngIsCloseRequested();
void ngExit();

} // namespace nijigenerate::core

namespace nijigenerate::core::window {
using ::nijigenerate::core::SDL3Binding;
using ::nijigenerate::core::ngSelectedSDL3Binding;
using ::nijigenerate::core::ngRequireSDL3Runtime;
using ::nijigenerate::core::ngShowStatsForNerds;
using ::nijigenerate::core::ngDifferenceAggregationDebugEnabled;
using ::nijigenerate::core::ngDifferenceAggregationTargetIndex;
using ::nijigenerate::core::ngDifferenceAggregationResolvedIndex;
using ::nijigenerate::core::ngDifferenceAggregationResult;
using ::nijigenerate::core::ngDifferenceAggregationResultValid;
using ::nijigenerate::core::ngDifferenceAggregationResultSerial;
using ::nijigenerate::core::ngDifferenceAggregationInitialResult;
using ::nijigenerate::core::WindowFileDropPayload;
using ::nijigenerate::core::ngRetainFileDropPayload;
using ::nijigenerate::core::ngIsWayland;
using ::nijigenerate::core::ngIsTilingWM;
using ::nijigenerate::core::ngOpenWindow;
using ::nijigenerate::core::ngCreateContext;
using ::nijigenerate::core::ngGetWindowPtr;
using ::nijigenerate::core::ngRefreshWindowTitle;
using ::nijigenerate::core::ngSetWindowTitle;
using ::nijigenerate::core::ngUpdateWindowTitleTick;
using ::nijigenerate::core::ngFinalize;
using ::nijigenerate::core::ngGetViewportDockSpace;
using ::nijigenerate::core::ngUiAccentColor;
using ::nijigenerate::core::ngInitStyling;
using ::nijigenerate::core::ngPushDarkColorScheme;
using ::nijigenerate::core::ngPushLightColorScheme;
using ::nijigenerate::core::ngPopColorScheme;
using ::nijigenerate::core::ngSetDarkMode;
using ::nijigenerate::core::ngGetDarkMode;
using ::nijigenerate::core::ngShouldProcess;
using ::nijigenerate::core::ngFinishFileDrag;
using ::nijigenerate::core::ngBeginLoopNoEv;
using ::nijigenerate::core::ngSetDefaultLayout;
using ::nijigenerate::core::ngBeginLoop;
using ::nijigenerate::core::ngEndLoop;
using ::nijigenerate::core::ngDebugImGuiState;
using ::nijigenerate::core::ngResetClearColor;
using ::nijigenerate::core::ngViewportHasTemporaryBackgroundColor;
using ::nijigenerate::core::ngViewportGetBackgroundColor;
using ::nijigenerate::core::ngViewportSetTemporaryBackgroundColor;
using ::nijigenerate::core::ngViewportClearTemporaryBackgroundColor;
using ::nijigenerate::core::ngIsCloseRequested;
using ::nijigenerate::core::ngExit;
#if defined(_WIN32)
using ::nijigenerate::core::DPIAwarenessContext;
using ::nijigenerate::core::ProcessDPIAwareness;
using ::nijigenerate::core::ngSetWin32DPIAwareness;
#endif
} // namespace nijigenerate::core::window
