#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <string_view>

namespace nijigenerate::io::pngheader {
void ngInspectPngDimensions(compat::RetainedStringSlice path, int& width, int& height);
// Explicit native views copy their bytes before entering the retained Source body.
void ngInspectPngDimensions(std::string_view path, int& width, int& height);
}
