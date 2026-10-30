#pragma once
#include "base.hpp"

namespace nijigenerate::viewport::common::mesheditor::tools {
namespace detail {
inline thread_local compat::Slice<std::shared_ptr<base::ToolInfo>> infoList;
}
compat::Slice<std::shared_ptr<base::ToolInfo>> incGetToolInfo();
inline std::shared_ptr<base::ToolInfo> ngGetToolInfoOf(enums::VertexToolMode mode) {
    const auto infos = detail::infoList;
    for (auto info : infos) if (compat::ngRequireObject(info)->mode() == mode) return info;
    return nullptr;
}
}
