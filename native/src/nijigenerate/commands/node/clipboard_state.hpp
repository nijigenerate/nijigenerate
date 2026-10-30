#pragma once
#include "nijigenerate/compat/slice.hpp"
#include <memory>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::commands::node::base {
using ClipboardNodeReference = std::shared_ptr<::nicxlive::core::nodes::Node>;
using ClipboardNodes = compat::Slice<ClipboardNodeReference>;

// Original module data is TLS; descriptors retain actual Node reference slots.
inline thread_local ClipboardNodes clipboardNodes;
} // namespace nijigenerate::commands::node::base
