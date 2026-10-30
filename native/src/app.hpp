#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::app {
using Arguments = compat::Slice<compat::RetainedStringSlice>;
int ngRunEditor(Arguments args);
void incUpdate();
void incUpdateNoEv();
}
