#pragma once

namespace nijigenerate::commands::automesh::config::detail {
// This is only the !CMD_LOG branch. The enabled writefln branch is a separate leaf.
template<class... T>
void cmdLogDisabled(T...) {}
} // namespace nijigenerate::commands::automesh::config::detail
