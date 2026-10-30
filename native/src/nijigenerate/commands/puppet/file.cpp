#include "file.hpp"
#include <limits>

namespace nijigenerate::commands::puppet::file::detail {
constinit double ngHeadlessVideoClock = std::numeric_limits<double>::quiet_NaN();
double ngHeadlessVideoNow() { return ngHeadlessVideoClock; }
}
