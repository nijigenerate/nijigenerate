#pragma once

#include <nijigenerate/core/asyncderivedupdate.hpp>

namespace nijigenerate::widgets::asyncderivedupdatedetails {
void drawAsyncDerivedUpdateDetailsUi(
    const compat::Slice<core::asyncderivedupdate::AsyncDerivedUpdateSnapshot>& snapshots);
}
