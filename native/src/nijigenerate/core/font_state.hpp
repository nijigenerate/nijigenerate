#pragma once
#include "font_entry.hpp"
#include <nijigenerate/compat/slice.hpp>
namespace nijigenerate::core::font::detail {
inline thread_local compat::Slice<FontEntry> families;
}
