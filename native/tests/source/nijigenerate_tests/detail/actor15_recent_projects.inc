// Complete original regression.d2719-2743, same regression::detail namespace.
void testProjectRecentFilesSettings() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::project::incGetPrevProjects;
    using nijigenerate::project::incAddPrevProject;
    namespace settingsApi = nijigenerate::core::settings;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    // Used std.conv.toChars!10(int).array closure. The authored loop supplies
    // nonnegative int values; retain its temp-string and exact copied range.
    const auto indexText = [](int input) {
        std::array<char, 11> buffer;
        std::size_t first = buffer.size();
        std::uint64_t value = static_cast<std::uint64_t>(input);
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return Text::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    auto oldProjects = incGetPrevProjects();
    nijigenerate::compat::ngRunWithScopeExit([&] {
        settingsApi::incSettingsSet(text("prev_projects"), settingsApi::SettingsStringArray{});
        for (int index = 0; index < 12; ++index) {
            auto path = concatenateSourceStrings({text("/tmp/nijigenerate-recent-"), indexText(index), text(".inx")});
            incAddPrevProject(settingsApi::SettingsString(path));
        }
        auto projects = incGetPrevProjects();
        check(projects.size() == 10, "recent project list should be pruned to 10 entries");
        check(projects[0].view().ends_with("nijigenerate-recent-11.inx"), "newest recent project should be first");
        check(projects[projects.size() - 1].view().ends_with("nijigenerate-recent-2.inx"),
            "oldest retained recent project should be the tenth newest");
        incAddPrevProject(settingsApi::SettingsString(text("/tmp/nijigenerate-recent-5.inx")));
        projects = incGetPrevProjects();
        check(projects.size() == 10, "adding a duplicate recent project should not grow the list");
        check(projects[0].view().ends_with("nijigenerate-recent-5.inx"),
            "duplicate recent project should be promoted to the front");
        std::size_t duplicateCount = 0;
        for (const auto project : projects) {
            if (project.view().ends_with("nijigenerate-recent-5.inx")) ++duplicateCount;
        }
        check(duplicateCount == 1, "duplicate recent project should appear exactly once");
    }, [&] { settingsApi::incSettingsSet(text("prev_projects"), oldProjects); });
}
