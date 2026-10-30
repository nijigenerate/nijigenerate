// Whole-source port of tests/autorig_alpha_support.d. BSD-2-Clause.
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>

namespace {
using namespace nijigenerate;
using autorig::deterministic::contracts::Point2;
using autorig::deterministic::contracts::ngRigNumber;
using autorig::deterministic::evidence::AlphaSupportIndex;

void sourceAssert(bool condition, const char* message, unsigned line) {
    if (!condition)
        throw compat::ngStaticError<compat::AssertError>(message, "tests/autorig_alpha_support.d", line);
}
}

int main(int count, char** args) {
    sourceAssert(count == 2, "Pass a real PSD-derived cloud JSON path", 13);
    auto source = autorig::json::ngParseAutoRigJson(compat::phobos::ngReadUtf8Text(compat::RetainedString(args[1])));
    compat::Slice<compat::Slice<Point2>> clouds;
    compat::Slice<Point2> queries;
    auto members = source.sourceObjectRef();
    for (auto& member : members) {
        auto cloud = member.value();
        compat::Slice<Point2> points;
        for (auto point : cloud.sourceArrayValue())
            points.append(Point2{ngRigNumber(point.sourceIndexValue(0)), ngRigNumber(point.sourceIndexValue(1))});
        if (points.empty()) continue;
        clouds.append(points);
        // Use observed artwork points, including points far outside other parts.
        for (std::size_t i = 0; i < std::min(std::size_t{8}, points.size()); ++i)
            queries.append(points[i * (points.size() - 1) /
                std::min(std::size_t{7}, points.size() - 1 ? points.size() - 1 : std::size_t{1})]);
    }
    const auto start = compat::SourceMonotonicClock::now();
    const auto elapsedMilliseconds = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            compat::SourceMonotonicClock::now() - start).count();
    };
    compat::Slice<compat::Slice<double>> distances;
    for (auto cloud : clouds) {
        auto index = std::allocate_shared<AlphaSupportIndex>(
            nicxlive::core::d_runtime::SourceAllocator<AlphaSupportIndex>{}, cloud);
        compat::Slice<double> row;
        for (auto query : queries) row.append(index->distance(query));
        distances.append(row);
    }
    const auto indexedMs = elapsedMilliseconds();
    std::size_t checked = 0;
    for (std::size_t i = 0; i < clouds.size(); ++i) {
        auto cloud = clouds[i];
        for (std::size_t j = 0; j < queries.size(); ++j) {
            auto query = queries[j];
            double best = std::numeric_limits<double>::infinity();
            for (auto point : cloud) {
                const auto dx = point[0] - query[0];
                const auto dy = point[1] - query[1];
                best = std::min(best, dx * dx + dy * dy);
            }
            sourceAssert(distances[i][j] == std::sqrt(best),
                "Nearest support differs from exhaustive search", 46);
            ++checked;
        }
    }
    // Every source %s argument here is integral; preserve its decimal spelling.
    const auto totalMs = elapsedMilliseconds();
    providers::mcp::stdio::standardOutput().writeln(
        "Real PSD supports=" + std::to_string(clouds.size()) + " queries=" + std::to_string(queries.size()) +
        " exact checks=" + std::to_string(checked) + " indexed_ms=" + std::to_string(indexedMs) +
        " total_ms=" + std::to_string(totalMs));
    return 0;
}
