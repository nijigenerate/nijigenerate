#include "nijigenerate/core/window_state_internal.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/platform/window_factory.hpp"
#include "nijigenerate/compat/source_c_string.hpp"
#include "nijigenerate/compat/detail/source_c_string_contract.hpp"
#include <bit>
#include <cstdint>

namespace nijigenerate::core::detail {

SDL_Window* tryCreateWindow(compat::RetainedStringSlice title, SDL_WindowFlags flags) {
    const compat::SourceCString terminatedTitle(title);
    compat::detail::ngSourceCStringPostconditions(title.view(), terminatedTitle.data());
    const auto width = settings::incSettingsGet<std::int32_t>(
        settings::SettingsString(compat::RetainedString::fromStaticLiteral("WinW")), 1280);
    const auto height = settings::incSettingsGet<std::int32_t>(
        settings::SettingsString(compat::RetainedString::fromStaticLiteral("WinH")), 800);
    // The primary uint casts retain these bits through the actual signed SDK ABI.
    const auto sdkWidth = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(width));
    const auto sdkHeight = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(height));
    return platform::ngCreateSDL3Window(terminatedTitle.data(), sdkWidth, sdkHeight, flags);
}

} // namespace nijigenerate::core::detail
