// Existing regression.d21483-21542; include inside regression::detail after original text helpers.
void testPlatformWindowsConsoleWritesAreGuarded() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    auto root = regressionSourceRoot(text(""));
    Slice<RetainedString> failures;
    const Slice<RetainedString> allowedConsoleProtocolModules{text("api/acp/echo_agent.d")};
    for (auto entry : paths::ngDirEntries(root, paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto rel = paths::ngRelativePath(entry.name().retainedString(), [&] { return root; });
        auto sourcePrefix = text("source/nijigenerate/");
        const auto sourcePrefixIndex = countUntilSourceString(rel, sourcePrefix);
        if (sourcePrefixIndex >= 0)
            rel = rel.subslice(static_cast<std::size_t>(sourcePrefixIndex) + sourcePrefix.size(), rel.size());
        bool allowed = false;
        for (const auto module : allowedConsoleProtocolModules) {
            if (module == rel) { allowed = true; break; }
        }
        if (allowed) continue;
        auto content = paths::ngReadUtf8Text(entry.name());
        std::int32_t depth = 0;
        Slice<std::int32_t> guardedDepths;
        bool inBlockComment = false;
        const auto lines = splitSourceLines(content);
        for (std::size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            auto line = lines[lineNo];
            while (guardedDepths.size() && depth < guardedDepths[guardedDepths.size() - 1])
                guardedDepths.resize(guardedDepths.size() - 1);
            const auto stripped = stripRegressionLeft(line.view());
            const bool commentOnly = inBlockComment || stripped.starts_with("//");
            const bool guardLine = rel.view() == "utils/crashdump.d" || stripped.starts_with("debug") ||
                stripped.starts_with("static if") || stripped.find("version(") != std::string_view::npos ||
                stripped.find("version (") != std::string_view::npos || stripped.starts_with("unittest");
            const bool guarded = guardedDepths.size() > 0 || guardLine;
            if (!commentOnly && !guarded && containsConsoleWriteCall(line.view())) {
                const auto retainedStripped = line.subslice(line.empty() ? 0 :
                    static_cast<std::size_t>(stripped.data() - line.data()), line.size());
                const std::array<format::TypedFormatArgument, 3> arguments{
                    format::ngFormatArgument(rel), format::ngFormatArgument(lineNo + 1),
                    format::ngFormatArgument(retainedStripped)};
                auto rendered = format::ngFormatTyped("%s:%s: %s", arguments);
                failures.append(nijigenerate::platform::detail::ngPathAllocation([&] {
                    return RetainedString(std::move(rendered));
                }));
            }
            if (stripped.find("/*") != std::string_view::npos) inBlockComment = true;
            if (inBlockComment && stripped.find("*/") != std::string_view::npos) inBlockComment = false;
            const auto delta = commentOnly ? 0 : braceDelta(line.view());
            const auto newDepth = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(depth) + static_cast<std::uint32_t>(delta));
            if (guardLine && delta > 0) guardedDepths.append(newDepth);
            depth = newDepth;
            while (guardedDepths.size() && depth < guardedDepths[guardedDepths.size() - 1])
                guardedDepths.resize(guardedDepths.size() - 1);
        }
    }
    const bool noFailures = failures.size() == 0;
    require(noFailures, concatenateSourceStrings({text("unguarded console write calls:\n"),
        joinSourceStrings({failures.data(), failures.size()}, text("\n"))}));
}

// Existing regression.d21544-21592; no constructor is added or invoked here.
void testPlatformStartupShutdownModuleConstructors() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    auto root = regressionSourceRoot(text(""));
    Slice<RetainedString> constructorIds;
    Slice<RetainedString> constructorLocations;
    for (auto entry : paths::ngDirEntries(root, paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto rel = normalizeRegressionSourcePath(entry.name().retainedString(), root);
        const auto lines = splitSourceLines(paths::ngReadUtf8Text(entry.name()));
        for (std::size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            auto line = lines[lineNo];
            const auto strippedView = stripRegressionLeft(line.view());
            const auto stripped = line.subslice(line.empty() ? 0 :
                static_cast<std::size_t>(strippedView.data() - line.data()), line.size());
            if (stripped.view().starts_with("//")) continue;
            if (stripped.view().find("static this()") != std::string_view::npos ||
                stripped.view().find("shared static this()") != std::string_view::npos ||
                stripped.view().find("static ~this()") != std::string_view::npos ||
                stripped.view().find("shared static ~this()") != std::string_view::npos) {
                const std::array<format::TypedFormatArgument, 2> idArguments{
                    format::ngFormatArgument(rel), format::ngFormatArgument(stripped)};
                auto id = format::ngFormatTyped("%s: %s", idArguments);
                constructorIds.append(nijigenerate::platform::detail::ngPathAllocation([&] {
                    return RetainedString(std::move(id));
                }));
                const std::array<format::TypedFormatArgument, 3> locationArguments{
                    format::ngFormatArgument(rel), format::ngFormatArgument(lineNo + 1),
                    format::ngFormatArgument(stripped)};
                auto location = format::ngFormatTyped("%s:%s: %s", locationArguments);
                constructorLocations.append(nijigenerate::platform::detail::ngPathAllocation([&] {
                    return RetainedString(std::move(location));
                }));
            }
        }
    }
    const Slice<RetainedString> allowedConstructors{
        text("commands/depth/bone.d: shared static this() {"),
        text("panels/agent.d: shared static ~this() {"),
        text("panels/nodes.d: static this() {"),
        text("panels/package.d: static this() {"),
        text("panels/resource.d: static this() {"),
        text("panels/timeline.d: static this() {"),
        text("viewport/package.d: static this() {"),
        text("widgets/output.d: static this() {")};
    nijigenerate::compat::RetainedStringMap<bool> seenConstructors;
    for (std::size_t i = 0; i < constructorIds.size(); ++i) {
        const auto id = constructorIds[i];
        bool allowed = false;
        for (const auto candidate : allowedConstructors) {
            if (candidate == id) { allowed = true; break; }
        }
        require(allowed, concatenateSourceStrings({text(
            "new module constructor/destructor must be reviewed for startup/shutdown cycles: "),
            constructorLocations[i]}));
        const bool unseen = !seenConstructors.contains(id.view());
        require(unseen, concatenateSourceStrings({text(
            "duplicate module constructor/destructor must be reviewed for startup/shutdown cycles: "),
            constructorLocations[i]}));
        seenConstructors.setRetained(id, true);
    }
}
