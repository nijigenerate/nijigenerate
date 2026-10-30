#pragma once
#include "nijigenerate/compat/retained_string.hpp"

namespace nijigenerate::widgets {
void ngStatusTooltip(compat::RetainedString action, compat::RetainedString key);
void ngStatusbar();
namespace statusbar {
using ::nijigenerate::widgets::ngStatusTooltip;
using ::nijigenerate::widgets::ngStatusbar;
}
}
