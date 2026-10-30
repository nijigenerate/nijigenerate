#include "depthmap_psd_targets.hpp"
#include "depthmap_psd_math.hpp"
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <string_view>
namespace nijigenerate::io::depthmap_psd::detail {
namespace {
bool separator(char c) {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
bool unc(std::string_view text) {
    return text.size() >= 3 && separator(text[0]) && separator(text[1]) && !separator(text[2]);
}
// Keep the platform's std.path.baseName rules aligned with the name helper.
std::string_view baseName(std::string_view path) {
    auto remaining = path;
#if defined(_WIN32)
    if (path.size() >= 2 && path[1] == ':') remaining.remove_prefix(2);
    else if (unc(path)) {
        std::size_t end = 3;
        while (end < path.size() && !separator(path[end])) ++end;
        if (end < path.size()) {
            auto share = end;
            do { ++share; } while (share < path.size() && separator(path[share]));
            if (share < path.size()) {
                do { ++share; } while (share < path.size() && !separator(path[share]));
                end = share;
            }
        }
        remaining.remove_prefix(end);
    }
    if (remaining.empty()) return unc(path) ? path.substr(0, 1) : std::string_view{};
#else
    if (remaining.empty()) return {};
#endif
    auto end = remaining.size();
    while (end && separator(remaining[end - 1])) --end;
    if (!end) return remaining.substr(0, 1);
    auto start = end;
    while (start && !separator(remaining[start - 1])
#if defined(_WIN32)
        && remaining[start - 1] != ':'
#endif
    ) --start;
    return remaining.substr(start, end - start);
}
float partLayerOverlapScore(PartRef part, int left, int top, int width, int height,
    int documentWidth, int documentHeight) {
    if (!part || width <= 0 || height <= 0 || documentWidth <= 0 || documentHeight <= 0) return 0.0f;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto bounds = part->bounds.value_or(std::array<float, 4>{nan, nan, nan, nan});
    const auto partLeft = bounds[0] + float(documentWidth) / 2.0f;
    const auto partTop = bounds[1] + float(documentHeight) / 2.0f;
    const auto partWidth = bounds[2] - bounds[0];
    const auto partHeight = bounds[3] - bounds[1];
    const auto layerLeft = static_cast<float>(left);
    const auto layerTop = static_cast<float>(top);
    const auto layerWidth = static_cast<float>(width);
    const auto layerHeight = static_cast<float>(height);
    return ngPsdDepthRectOverlapScore(layerLeft, layerTop, layerWidth, layerHeight,
        partLeft, partTop, partWidth, partHeight);
}
}
compat::Slice<Candidate> matchCandidates(PuppetRef puppet, compat::RetainedString layerPath,
    compat::RetainedString layerName, PsdDepthImportSettings& settings,
    int left, int top, int width, int height, int documentWidth, int documentHeight, bool useBoundsFallback) {
    using namespace nicxlive::core::nodes;
    compat::Slice<Candidate> candidates;
    for (auto part : puppet->findNodesType<Part>(puppet->root)) {
        const auto path = activeArtLayerPath(part);
        if (path.empty()) continue;
        if (path == layerPath) candidates.append({part, 0, 1.0f});
        else if (baseName(path.view()) == layerName.view()) candidates.append({part, 1, 1.0f});
        else if (part->getName() == layerName.view()) candidates.append({part, 2, 1.0f});
        else {
            const auto normalizedPath = ngPsdDepthNormalizeLayerName(path);
            const auto normalizedLayerName = ngPsdDepthNormalizeLayerName(layerName);
            bool normalizedMatch = normalizedPath == normalizedLayerName;
            if (!normalizedMatch) {
                const auto normalizedPartName = ngPsdDepthNormalizeLayerName(ext::nodes::ngRetainNodeName(part->getName()));
                const auto normalizedLayerNameAgain = ngPsdDepthNormalizeLayerName(layerName);
                normalizedMatch = normalizedPartName == normalizedLayerNameAgain;
            }
            if (normalizedMatch) candidates.append({part, 3, 1.0f});
            else if (useBoundsFallback) {
                const float overlap = partLayerOverlapScore(part, left, top, width, height, documentWidth, documentHeight);
                if (overlap > 0.0f) candidates.append({part, 4, overlap});
            }
        }
    }
    if (settings.matchDirectGridName) {
        for (auto grid : puppet->findNodesType<GridDeformer>(puppet->root))
            if (grid->getName() == layerName.view()) candidates.append({grid, 5, 1.0f});
        for (auto path : puppet->findNodesType<PathDeformer>(puppet->root))
            if (path->getName() == layerName.view()) candidates.append({path, 5, 1.0f});
    }
    if (candidates.empty()) return candidates;
    auto priority = candidates[0].priority;
    for (auto value : candidates) if (value.priority < priority) priority = value.priority;
    compat::Slice<Candidate> best;
    for (auto value : candidates) if (value.priority == priority) best.append(value);
    if (best.size() > 1) {
        float overlap = best[0].overlapScore;
        for (auto value : best) if (value.overlapScore > overlap) overlap = value.overlapScore;
        compat::Slice<Candidate> overlapBest;
        for (auto value : best) if (value.overlapScore == overlap) overlapBest.append(value);
        best = overlapBest;
    }
    return best;
}
}
