#include "core/mainfont.hpp"

namespace nijigenerate::core {
namespace {
thread_local ImFont* mainFont = nullptr;
}

ImFont* incMainFont() {
    return mainFont;
}
} // namespace nijigenerate::core
