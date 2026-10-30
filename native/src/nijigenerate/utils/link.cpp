/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
*/
#include "link.hpp"
#include <nijigenerate/platform/browser.hpp>

namespace nijigenerate::utils::link {
void ngOpenLink(std::string_view link) {
    platform::ngBrowse(link);
}
}
