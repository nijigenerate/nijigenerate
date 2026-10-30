// Existing regression.d21063-21189; include once outside all namespaces,
// after the original Scenario type, inventories, and module-private helpers.
#include "root_source_command_inventory.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory_range.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <array>
#include <span>

namespace nijigenerate_tests::regression::detail {
void testCoverageSourceCommandInventory() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using namespace nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    auto root = regressionSourceRoot(text(""));
    Slice<RetainedString> sourceFiles;
    Slice<RetainedString> failures;
    for (auto entry : ngDirEntries(root, SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        sourceFiles.append(normalizeRegressionSourcePath(entry.name().retainedString(), root));
    }
    sortRegressionStrings(sourceFiles);

    const Slice<RetainedString> requiredSourceRoots{
        text("actions"), text("api"), text("atlas"), text("backend"), text("commands"), text("core"),
        text("ext"), text("io"), text("panels"), text("project.d"), text("utils"), text("viewport"),
        text("widgets"), text("windows")};
    for (const auto required : requiredSourceRoots) {
        bool found = false;
        for (const auto rel : sourceFiles) {
            if (rel.view() == required.view() ||
                rel.view().starts_with(concatenateSourceStrings({required, text("/")}).view())) {
                found = true;
                break;
            }
        }
        if (!found) failures.append(concatenateSourceStrings({text("missing source root in inventory: "), required}));
    }
    const Slice<RetainedString> requiredScenarioPrefixes{
        text("coverage"), text("project"), text("io"), text("node"), text("core"), text("inspectors"),
        text("part"), text("parameter"), text("animation"), text("api"), text("mesh"), text("deform"),
        text("depth"), text("depthbone"), text("automesh"), text("simplephysics"), text("viewport"),
        text("render"), text("settings"), text("panels"), text("tools"), text("windows"), text("widgets"),
        text("i18n"), text("platform"), text("undo")};
    for (const auto prefix : requiredScenarioPrefixes) {
        if (!hasScenarioPrefix(prefix))
            failures.append(concatenateSourceStrings({text("missing scenario family: "), prefix}));
    }
    for (const auto scenario : allScenarios()) {
        if (scenario.status.view() == "manual")
            failures.append(concatenateSourceStrings({
                text("manual status is not allowed; use computer-use or automated: "), scenario.id}));
        if (scenario.status.view() == pending)
            failures.append(concatenateSourceStrings({
                text("pending status is not allowed; use computer-use or automated: "), scenario.id}));
    }
    for (const auto id : requiredSourceScenarioIds) {
        bool found = false;
        for (const auto scenario : allScenarios()) {
            if (scenario.id.view() == id.view()) {
                found = true;
                break;
            }
        }
        if (!found)
            failures.append(concatenateSourceStrings({text("missing required source-derived scenario: "), id}));
    }
    for (const auto rel : sourceFiles) {
        auto prefix = scenarioPrefixForSourceModule(rel.view());
        if (prefix.empty())
            failures.append(concatenateSourceStrings({text("source module has no scenario owner: "), rel}));
        else if (!hasScenarioPrefix(prefix))
            failures.append(concatenateSourceStrings({
                text("source module owner has no scenario family: "), rel, text(" -> "), prefix}));
    }
    for (const auto rel : sourceFiles) {
        if (!rel.view().starts_with("commands/")) continue;
        const std::array segments{root, rel};
        auto sourceText = ngReadUtf8Text(ngBuildPath(segments));
        if (countUntilRegressionNeedle(sourceText.view(), ": ExCommand") < 0 &&
            countUntilRegressionNeedle(sourceText.view(), "mixin(\"enum AutoMeshTypedCommand") < 0) continue;
        auto prefix = scenarioPrefixForCommandModule(rel.view());
        if (prefix.empty())
            failures.append(concatenateSourceStrings({text("command module has no coverage owner: "), rel}));
        else if (!hasScenarioPrefix(prefix))
            failures.append(concatenateSourceStrings({
                text("command module owner has no scenario family: "), rel, text(" -> "), prefix}));
    }
    require(sourceFiles.size() >= 265, text("source inventory should include all nijigenerate modules"));
    auto joined = joinSourceStrings(std::span<const RetainedString>(failures.data(), failures.size()), text("\n"));
    require(failures.empty(), concatenateSourceStrings({text("coverage inventory failures:\n"), joined}));
}
}
