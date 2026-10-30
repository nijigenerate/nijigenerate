#pragma once
#include <string_view>

namespace nijigenerate::platform {
// The OS launch boundary consumes the borrowed descriptor during this call.
void ngBrowse(std::string_view link);
}
