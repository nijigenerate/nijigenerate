// Existing regression.d21271-21294 and 21428-21461 only.
void testUndoDirectMutationAudit() {
    using Text = nijigenerate::compat::RetainedString;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    auto root = regressionSourceRoot(text(""));
    nijigenerate::compat::Slice<Text> failures;
    for (auto prefix : std::array{text("commands"), text("panels"), text("viewport"), text("windows")}) {
        const std::array segments{root, prefix};
        for (auto entry : paths::ngDirEntries(paths::ngBuildPath(segments), paths::SpanMode::depth)) {
            if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
            auto rel = normalizeRegressionSourcePath(entry.name().retainedString(), root);
            auto lines = splitSourceLines(paths::ngReadUtf8Text(entry.name()));
            for (std::size_t i = 0; i < lines.size(); ++i) {
                auto line = lines[i];
                if (!containsDirectMutationNeedle(line.view())) continue;
                if (stripRegressionLeft(line.view()).starts_with("//")) continue;
                if (!isAllowedDirectMutation(rel.view(), line.view())) {
                    const auto view = stripRegressionLeft(line.view());
                    auto stripped = line.subslice(line.empty() ? 0 :
                        static_cast<std::size_t>(view.data() - line.data()), line.size());
                    const std::array<format::TypedFormatArgument, 3> args{
                        format::ngFormatArgument(rel), format::ngFormatArgument(i + 1), format::ngFormatArgument(stripped)};
                    auto rendered = format::ngFormatTyped("%s:%s: %s", args);
                    failures.append(nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); }));
                }
            }
        }
    }
    const bool valid = failures.empty();
    auto joined = joinSourceStrings(std::span<const Text>(failures.data(), failures.size()), text("\n"));
    auto message = concatenateSourceStrings({text("unreviewed direct model/parameter/deformation mutations:\n"), joined});
    require(valid, message);
}

void testUndoCommandActionsAudit() {
    using Text = nijigenerate::compat::RetainedString;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    auto root = regressionSourceRoot(text("commands"));
    nijigenerate::compat::Slice<Text> failures;
    std::size_t commandClasses = 0;
    std::size_t undoableClasses = 0;
    for (auto entry : paths::ngDirEntries(root, paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto rel = normalizeRegressionSourcePath(entry.name().retainedString(), regressionSourceRoot(text("")));
        auto content = paths::ngReadUtf8Text(entry.name());
        auto moduleHasUndoPath = commandModuleHasUndoPath(content.view());
        const auto lines = splitSourceLines(content);
        for (std::size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            auto line = lines[lineNo];
            if (!isRegressionCommandClassLine(line.view())) continue;
            auto className = extractRegressionClassName(line);
            if (className.empty()) continue;
            ++commandClasses;
            if (!commandClassShouldBeUndoable(className.view())) continue;
            ++undoableClasses;
            if (!moduleHasUndoPath) {
                const std::array<format::TypedFormatArgument, 3> args{
                    format::ngFormatArgument(rel), format::ngFormatArgument(lineNo + 1), format::ngFormatArgument(className)};
                auto rendered = format::ngFormatTyped("%s:%s: %s has no detected undo/action path", args);
                failures.append(nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); }));
            }
        }
    }
    require(commandClasses >= 140, text("command action audit should see all command classes"));
    require(undoableClasses >= 70, text("command action audit should classify mutating command classes"));
    const bool valid = failures.empty();
    auto joined = joinSourceStrings(std::span<const Text>(failures.data(), failures.size()), text("\n"));
    auto message = concatenateSourceStrings({text("commands missing undo/action coverage:\n"), joined});
    require(valid, message);
}
