#pragma once

namespace nijigenerate::commands::detail {
namespace {

// This helper represents only the !CMD_LOG branch, not the writefln branch.
template<class... T>
void cmdLogDisabled(T...) {}

} // namespace
} // namespace nijigenerate::commands::detail
