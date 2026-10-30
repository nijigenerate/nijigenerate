#pragma once

#include <nijigenerate/windows/psddepthmap.hpp>
#include <memory>

namespace nijigenerate::windows::psddepthmap::detail {

// Module-private storage shared by the actual getter and dialog lifecycle bodies.
// This is process-wide __gshared state, not TLS or an atomic/mutex protocol.
extern std::shared_ptr<PSDDepthMapWindow> activePsdDepthMapWindow;

} // namespace nijigenerate::windows::psddepthmap::detail
