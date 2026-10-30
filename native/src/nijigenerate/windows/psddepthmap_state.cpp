#include "detail/psddepthmap_state.hpp"

namespace nijigenerate::windows::psddepthmap {
namespace detail {

std::shared_ptr<PSDDepthMapWindow> activePsdDepthMapWindow;

} // namespace detail

std::shared_ptr<PSDDepthMapWindow> ngActivePsdDepthMapWindow() {
    return detail::activePsdDepthMapWindow;
}

} // namespace nijigenerate::windows::psddepthmap
