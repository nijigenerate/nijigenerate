#include "controls_profile.hpp"
#include "controls_profile_sort.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <cmath>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <vector>

namespace nijigenerate::autorig::deterministic::controls_detail {

Profile profile(compat::Slice<contracts::Point2> support, Frame frame) {
    compat::Slice<contracts::Point2> local;
    for (const auto point : support) local.append(frame.local(point));
    Profile result;
    for (std::size_t i = 0; i < 65; ++i) {
        const double station = -frame.width / 2 + frame.width * i / 64;
        compat::Slice<double> heights;
        for (const auto point : local)
            if (std::abs(point[0] - station) <= frame.width / 32) heights.append(point[1]);
        if (heights.empty()) {
            auto sorted = [&] {
                auto duplicate = local.duplicate();
                controls_profile_sort::sort(
                    std::span<contracts::Point2>(duplicate.data(), duplicate.size()), station);
                if (duplicate.empty()) return compat::Slice<contracts::Point2>{};
                try {
                    return compat::Slice<contracts::Point2>(
                        std::vector<contracts::Point2>(duplicate.begin(), duplicate.end()));
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            const auto count = std::max<std::size_t>(1, sorted.size() / 100);
            for (const auto point : sorted.slice(0, count)) heights.append(point[1]);
        }
        result.axis.append(station);
        const auto values = std::span<const double>(heights.data(), heights.size());
        result.upper.append(percentile(values, .02));
        result.lower.append(percentile(values, .98));
    }
    return result;
}

} // namespace nijigenerate::autorig::deterministic::controls_detail
