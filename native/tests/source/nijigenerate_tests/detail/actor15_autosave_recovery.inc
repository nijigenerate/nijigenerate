// Complete original regression.d2663-2717, same regression::detail namespace.
void testProjectAutosaveRecoveryRecords() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::puppet::file::SaveFileCommand;
    namespace paths = nijigenerate::compat::phobos;
    namespace autosave = nijigenerate::io::autosave;
    namespace settings = nijigenerate::core::settings;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    const std::array fixtureSegments{paths::ngTempDirectory(), text("nijigenerate-regression-autosave")};
    auto fixtureDir = paths::ngBuildPath(fixtureSegments);
    if (paths::ngFileExists(fixtureDir.view())) paths::ngRmdirRecurse(fixtureDir);
    paths::ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        settings::incSettingsSet(text("prev_autosaves"), settings::SettingsStringArray{});
        settings::incSettingsSet(text("prev_autosave_mainpaths"), settings::SettingsStringArray{});
        const std::array saveSegments{fixtureDir, text("autosave-roundtrip")};
        auto saveBase = paths::ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, text(".inx")});
        auto autosaveDir = autosave::getAutosaveDir(saveBase);
        if (paths::ngFileExists(autosaveDir.view())) paths::ngRmdirRecurse(autosaveDir);
        nijigenerate::compat::ngRunWithScopeExit([&] {
            auto node = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            node->setName(NullableString::fromStaticLiteral("Autosave Node"));
            auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            check(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx)->succeeded,
                "SaveFileCommand should succeed before autosave");
            check(paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()),
                "manual save should create the main .inx file");
            check(nijigenerate::project::incProjectPath().view() == saveBase.view(),
                "manual save should keep the active project base path");
            autosave::incSetAutosaveFileLimit(2);
            autosave::incAutosaveProject(saveBase);
            check(paths::ngFileExists(autosaveDir.view()), "autosave should create the autosave directory");
            auto backups = autosave::currentBackups(autosaveDir);
            check(backups.size() == 1, "autosave should create one backup file");
            auto backupPath = backups[0].name();
            check(paths::ngFileExists(backupPath.view()) && paths::ngIsFile(backupPath.view()),
                "autosave backup path should exist");
            check(autosave::incCheckLockfile(saveBase), "autosave should create a lockfile for recovery detection");
            auto records = autosave::incGetPrevAutosaves();
            check(records.size() == 1, "autosave should register one recovery record");
            check(records[0].autosavePath.view() == backupPath.view(),
                "recovery record should point at the backup file");
            check(records[0].mainsavePath.view() == saveBase.view(),
                "recovery record should point at the main project base path");
            autosave::incReleaseLockfile();
            check(!autosave::incCheckLockfile(saveBase), "normal release should remove the autosave recovery lockfile");
            nijigenerate::platform::ngAutosaveRemove(backupPath);
            autosave::incPruneAutosaveList();
            check(autosave::incGetPrevAutosaves().size() == 0, "prune should remove stale autosave recovery records");
        }, [&] {
            if (paths::ngFileExists(autosaveDir.view())) paths::ngRmdirRecurse(autosaveDir);
        });
    }, [&] {
        if (paths::ngFileExists(fixtureDir.view())) paths::ngRmdirRecurse(fixtureDir);
    });
}
