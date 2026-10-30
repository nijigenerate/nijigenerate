#pragma once
#include <nijigenerate/compat/retained_string.hpp>

// These definitions correspond to the D module's version identifiers.
#if defined(NG_USE_UI_SCALING) && defined(NG_NO_UI_SCALING)
#error "UseUIScaling and NoUIScaling together leave the D uiScale variable undefined"
#endif

namespace nijigenerate::core::dpi {
#if defined(NG_USE_UI_SCALING)
void incInitDPIScaling();
#endif
void incSetUIScale(float scale);
float incGetUIScaleFont();
float incGetUIScale();
compat::RetainedString incGetUIScaleText();
} // namespace nijigenerate::core::dpi
