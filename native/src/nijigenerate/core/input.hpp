#pragma once
#include "input_poll.hpp"
#include "input_mouse_position.hpp"
#include "input_events.hpp"
#include "input_key_up.hpp"
#include "input_key_scancode.hpp"
#include "input_shortcut_template.hpp"
#include "input_modifier_labels.hpp"
#include "input_format_shortcut.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <math/camera.hpp>
#include <memory>

namespace nijigenerate::core::input {
nicxlive::core::math::Vec2 WorldToViewport(float x, float y,
    std::shared_ptr<nicxlive::core::math::Camera> camera = {});
void ngInputSetViewportMouse(float x, float y);
bool ngInputIsMouseDown(int index);
bool ngInputIsPopupRequested();
bool ngShortcut(compat::RetainedStringSlice shortcut, bool repeat = false);
// Host callers without retained storage copy at this explicit compatibility boundary.
bool ngShortcut(std::string_view shortcut, bool repeat = false);
}
