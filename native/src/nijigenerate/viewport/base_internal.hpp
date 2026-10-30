#pragma once
#include "base.hpp"

namespace nijigenerate::viewport::base::detail {
inline constexpr std::int32_t handleSize = 24;
// Module-private owner slot. This never constructs or substitutes a MainViewport.
std::shared_ptr<MainViewport>& ngMainViewportStorage();
}
