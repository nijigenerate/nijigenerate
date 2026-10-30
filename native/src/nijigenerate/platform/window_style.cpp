#include "window_style.hpp"

namespace nijigenerate::platform {
bool ngCenterWindowTitle() {
#if defined(__APPLE__)
    return true;
#else
    return false;
#endif
}
}
