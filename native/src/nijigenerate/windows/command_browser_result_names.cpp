#include "command_browser.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::windows::command_browser::detail {
BrowserText browserResultTypeName(const std::shared_ptr<commands::CommandResult>& result) {
    return BrowserText(compat::ngRequireObject(result)->sourceClassInfoName());
}
}
