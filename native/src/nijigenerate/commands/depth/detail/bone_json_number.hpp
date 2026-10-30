#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include "phobos_float_parse.hpp"
#include <nijigenerate/core/settings_value.hpp>

namespace nijigenerate::commands::depth::bone {
namespace {

float jsonNumber(::nijigenerate::core::settings::SettingsJsonValue value, float fallback) {
    if (value.is_int64()) return static_cast<float>(value.as_int64());
    if (value.is_uint64()) return static_cast<float>(value.as_uint64());
    if (value.is_double()) return static_cast<float>(value.as_double());
    if (value.is_string()) {
        const auto text = value.as_string().view();
        return text.empty() ? fallback : phobos_float_detail::parseStringFloat(text);
    }
    if (value.is_bool()) return value.as_bool() ? 1.0f : 0.0f;
    return fallback;
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
