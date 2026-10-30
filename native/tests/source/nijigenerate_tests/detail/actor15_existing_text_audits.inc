// Complete existing regression.d21779-21809 and 21811-21828 only.
// Include once inside regression::detail after the original string/UI helpers.
void testI18nVisibleUiStringsAreWrapped() {
    using Text = nijigenerate::compat::RetainedString;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    auto root = regressionSourceRoot(text(""));
    nijigenerate::compat::Slice<Text> failures;
    for (auto entry : paths::ngDirEntries(root, paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto rel = paths::ngRelativePath(entry.name().retainedString(), [&] { return root; });
        auto sourcePrefix = text("source/nijigenerate/");
        auto sourcePrefixIndex = countUntilSourceString(rel, sourcePrefix);
        if (sourcePrefixIndex >= 0) rel = rel.subslice(static_cast<std::size_t>(sourcePrefixIndex) + sourcePrefix.size(), rel.size());
        auto content = paths::ngReadUtf8Text(entry.name());
        bool inBlockComment = false;
        const auto lines = splitSourceLines(content);
        for (std::size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            auto line = lines[lineNo];
            const auto strippedView = stripRegressionLeft(line.view());
            auto stripped = line.subslice(line.empty() ? 0 :
                static_cast<std::size_t>(strippedView.data() - line.data()), line.size());
            auto commentOnly = inBlockComment || stripped.view().starts_with("//");
            if (!commentOnly && containsUnwrappedVisibleUiLiteral(stripped.view())) {
                const std::array<format::TypedFormatArgument, 3> arguments{
                    format::ngFormatArgument(rel), format::ngFormatArgument(lineNo + 1), format::ngFormatArgument(stripped)};
                auto rendered = format::ngFormatTyped("%s:%s: %s", arguments);
                auto message = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
                failures.append(message);
            }
            if (stripped.view().find("/*") != std::string_view::npos) inBlockComment = true;
            if (inBlockComment && stripped.view().find("*/") != std::string_view::npos) inBlockComment = false;
        }
    }
    const bool valid = failures.empty();
    const auto joined = joinSourceStrings(std::span<const Text>(failures.data(), failures.size()), text("\n"));
    const auto message = concatenateSourceStrings({text("unwrapped visible UI strings:\n"), joined});
    require(valid, message);
}

void testInspectorFormatStringsDoNotEscapeNumericFormats() {
    using Text = nijigenerate::compat::RetainedString;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    auto root = regressionSourceRoot(text("panels/inspector"));
    nijigenerate::compat::Slice<Text> failures;
    for (auto entry : paths::ngDirEntries(root, paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto content = paths::ngReadUtf8Text(entry.name());
        const auto lines = splitSourceLines(content);
        for (std::size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            auto line = lines[lineNo];
            if (line.view().find("\"%%0") != std::string_view::npos || line.view().find("\"%%.") != std::string_view::npos) {
                const auto relative = paths::ngRelativePath(entry.name().retainedString(), [&] { return regressionSourceRoot(text("")); });
                const auto number = lineNo + 1;
                const auto strippedView = stripRegressionLeft(line.view());
                auto stripped = line.subslice(line.empty() ? 0 :
                    static_cast<std::size_t>(strippedView.data() - line.data()), line.size());
                const std::array<format::TypedFormatArgument, 3> arguments{
                    format::ngFormatArgument(relative), format::ngFormatArgument(number), format::ngFormatArgument(stripped)};
                auto rendered = format::ngFormatTyped("%s:%s: %s", arguments);
                auto message = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
                failures.append(message);
            }
        }
    }
    const bool valid = failures.empty();
    const auto joined = joinSourceStrings(std::span<const Text>(failures.data(), failures.size()), text("\n"));
    const auto message = concatenateSourceStrings({text("escaped inspector numeric format strings:\n"), joined});
    require(valid, message);
}
