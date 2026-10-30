#include "nijigenerate/widgets/statusbar_internal.hpp"

namespace nijigenerate::widgets::statusbar::detail {
std::size_t& itemIndex() {
    static thread_local std::size_t value = 0;
    return value;
}
}
