#include "dpi.hpp"
#include "settings.hpp"
#include "translation.hpp"
#include "typed_format.hpp"
#include <nijigenerate/compat/numeric.hpp>
#include <array>
#include <limits>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::core::dpi {
#if defined(NG_USE_UI_SCALING)
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "The D DPI cache requires an IEEE 754 binary32 float");
// D module variables are thread local and float.init is NaN.
thread_local float uiScale = std::numeric_limits<float>::quiet_NaN();
}
void incInitDPIScaling() {
    uiScale = settings::incSettingsGet<float>(compat::RetainedString::fromStaticLiteral("UIScale"), 1.0f);
}
#endif

void incSetUIScale(float scale) {
#if defined(NG_USE_UI_SCALING)
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("UIScale"), scale);
    uiScale = scale;
#else
    (void)scale;
#endif
}

float incGetUIScaleFont() {
#if defined(NG_USE_UI_SCALING)
    return incGetUIScale() / 2;
#else
    return 0.5f;
#endif
}

float incGetUIScale() {
#if defined(NG_USE_UI_SCALING) && !defined(NG_OSX)
    return uiScale;
#else
    return 1;
#endif
}

compat::RetainedString incGetUIScaleText() {
#if defined(NG_USE_UI_SCALING)
    const std::array<translation::TypedFormatArgument, 1> arguments{
        translation::ngFormatArgument(compat::ngFloatToInt32(uiScale * 100.0f))};
    auto text = translation::ngFormatTyped("%s%%", arguments);
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
#else
    return translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("100% (Locked)"));
#endif
}
} // namespace nijigenerate::core::dpi
