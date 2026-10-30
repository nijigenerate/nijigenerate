#include "detail/unique_psd_path.hpp"
#include "detail/psd_depth_allocation.hpp"
#include <array>
#include <charconv>
#include <limits>
#include <string>
#include <utility>

namespace nijigenerate::io::depthmap_psd::detail {

compat::RetainedString uniquePsdLayerPath(compat::RetainedString path, PsdLayerPathOccurrences& occurrences) {
    const auto previous = occurrences.findRetained(path.view());
    const std::size_t occurrence = previous ? *previous + std::size_t{1} : std::size_t{1};
    occurrences.setRetained(path, occurrence);
    if (occurrence == 1) return path;

    // Every size_t fits in digits10 + 1 unsigned decimal digits.
    std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> digits{};
    const auto decimal = std::to_chars(digits.data(), digits.data() + digits.size(), occurrence);
    return allocateImportBytes([&] {
        std::string result;
        if (!path.empty()) result.assign(path.data(), path.size());
        result.append(" [#");
        result.append(digits.data(), static_cast<std::size_t>(decimal.ptr - digits.data()));
        result.push_back(']');
        return compat::RetainedString(std::move(result));
    });
}

}
