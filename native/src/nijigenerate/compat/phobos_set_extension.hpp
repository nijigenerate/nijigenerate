#pragma once
// Used immutable char[] std.path.setExtension/withExtension, D Language Foundation.
// BSL-1.0; see port/licenses/phobos-psd-file.txt and installed std/path.d.
#include "retained_string.hpp"
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <string>
#include <utility>

namespace nijigenerate::compat::phobos {
inline RetainedString ngSetExtension(RetainedString path, RetainedString extension) {
    const auto stem = platform::ngAutosaveStripExtension(path.view());
    // The immutable overload returns the borrowed strip descriptor for empty ext.
    if (extension.empty()) return path.subslice(0, stem.size());
    return platform::detail::ngPathAllocation([&] {
        std::string result;
        if (!stem.empty()) result.append(stem.data(), stem.size());
        if (extension.data()[0] != '.') result.push_back('.');
        result.append(extension.data(), extension.size());
        return RetainedString(std::move(result));
    });
}
} // namespace nijigenerate::compat::phobos
