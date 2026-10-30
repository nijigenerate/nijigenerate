// Original regression.d19214-19251 and the additional authored bodies below.
// Include once inside regression::detail after require/regressionSourceRoot.
// The original helper body is supplied once by root_viewport_settings_composite.inc.
void clearAllShortcuts();

void testDefaultShortcutRegistration() {
    using nijigenerate::compat::RetainedString;
    namespace shortcuts = nijigenerate::core::shortcut;
    namespace files = nijigenerate::commands::puppet::file;
    namespace edits = nijigenerate::commands::puppet::edit;
    namespace palette = nijigenerate::commands::viewport::palette;
    const auto commandAt = [](const auto& table, auto key, std::size_t line) {
        const auto found = table.find(key);
        if (found == table.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return found->second;
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    clearAllShortcuts();
    shortcuts::ngRegisterDefaultShortcuts();
    check(shortcuts::ngShortcutFor(commandAt(files::commands, files::FileCommand::NewFile, 19218)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-N">::value.view(), "default shortcut should bind NewFile");
    check(shortcuts::ngShortcutFor(commandAt(files::commands, files::FileCommand::ShowOpenFileDialog, 19220)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-O">::value.view(), "default shortcut should bind open dialog");
    check(shortcuts::ngShortcutFor(commandAt(files::commands, files::FileCommand::ShowSaveFileDialog, 19222)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-S">::value.view(), "default shortcut should bind save dialog");
    check(shortcuts::ngShortcutFor(commandAt(files::commands, files::FileCommand::ShowSaveFileAsDialog, 19224)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-Shift-S">::value.view(), "default shortcut should bind save-as dialog");
    check(commandAt(files::commands, files::FileCommand::ShowImportPSDDepthMapDialog, 19226) != nullptr &&
        !commandAt(files::commands, files::FileCommand::ShowImportPSDDepthMapDialog, 19227)->label().empty(),
        "PSD depth map import entry command should be registered with a menu label");
    const std::array menuSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("widgets")),
        RetainedString::fromStaticLiteral("mainmenu.d")};
    auto mainMenuSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(menuSegments));
    check(mainMenuSource.view().find("FileCommand.ShowImportPSDDepthMapDialog") != std::string_view::npos,
        "Import menu should expose the PSD/PNG depth source import entry");
    check(mainMenuSource.view().find("DepthDrawWindow") == std::string_view::npos &&
        mainMenuSource.view().find("Open DepthDraw Source") == std::string_view::npos &&
        mainMenuSource.view().find("ShowDepthDraw") == std::string_view::npos,
        "Import menu must not expose standalone DepthDraw as a second product workflow");
    const std::array commandSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("commands")),
        RetainedString::fromStaticLiteral("puppet"), RetainedString::fromStaticLiteral("file.d")};
    auto fileCommandSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(commandSegments));
    check(fileCommandSource.view().find("[\"*.psd\", \"*.png\"]") != std::string_view::npos &&
        fileCommandSource.view().find("Depth Source (*.psd, *.png)") != std::string_view::npos,
        "PSD depth map import dialog must accept PSD and PNG sources through the single product entry");
    check(fileCommandSource.view().find("Open DepthDraw Source") == std::string_view::npos &&
        fileCommandSource.view().find("ShowDepthDraw") == std::string_view::npos,
        "File import dialog commands must not expose standalone DepthDraw as a second product workflow");
    check(shortcuts::ngShortcutFor(commandAt(edits::commands, edits::EditCommand::Undo, 19243)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-Z">::value.view(), "default shortcut should bind undo");
    check(shortcuts::ngShortcutFor(commandAt(edits::commands, edits::EditCommand::Redo, 19245)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-Shift-Z">::value.view(), "default shortcut should bind redo");
    check(shortcuts::ngShortcutFor(commandAt(palette::commands, palette::PaletteCommand::ListCommand, 19247)).view() ==
        nijigenerate::core::input::ShortcutSpec<"Ctrl-Shift-P">::value.view(), "default shortcut should bind command palette");
    clearAllShortcuts();
}

// Complete existing regression.d18830-18861; the helper above is shared.
void testShortcutSettingsConflictAndReload() {
    using nijigenerate::compat::RetainedString;
    namespace shortcuts = nijigenerate::core::shortcut;
    namespace nodes = nijigenerate::commands::node::node;
    namespace settingsApi = nijigenerate::core::settings;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto commandAt = [](auto key, std::size_t line) {
        const auto found = nodes::commands.find(key);
        if (found == nodes::commands.end())
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
                RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return found->second;
    };
    resetCase();
    clearAllShortcuts();
    auto addCommand = commandAt(nodes::NodeCommand::AddNode, 18834);
    auto deleteCommand = commandAt(nodes::NodeCommand::DeleteNode, 18835);
    check(addCommand != nullptr && deleteCommand != nullptr, "shortcut test requires initialized node commands");
    shortcuts::ngRegisterShortcut(RetainedString::fromStaticLiteral("Ctrl+Alt+N"), addCommand);
    check(shortcuts::ngShortcutFor(addCommand).view() == "Ctrl+Alt+N", "shortcut registration should bind command");
    shortcuts::ngRegisterShortcut(RetainedString::fromStaticLiteral("Ctrl+Alt+N"), deleteCommand);
    check(shortcuts::ngShortcutFor(addCommand).empty(), "shortcut conflict should clear old command");
    check(shortcuts::ngShortcutFor(deleteCommand).view() == "Ctrl+Alt+N", "shortcut conflict should bind new command");
    shortcuts::ngSaveShortcutsToSettings();
    auto saved = settingsApi::incSettingsGet<settingsApi::SettingsStringMap>(
        RetainedString::fromStaticLiteral("Shortcuts"));
    bool foundSavedShortcut = false;
    for (const auto& [id, shortcut] : saved) {
        (void)id;
        if (shortcut.view() == "Ctrl+Alt+N") foundSavedShortcut = true;
    }
    check(foundSavedShortcut, "shortcut save should persist shortcut string with a loadable command id");
    clearAllShortcuts();
    check(shortcuts::ngShortcutFor(deleteCommand).empty(), "shortcut clear should remove binding");
    shortcuts::ngLoadShortcutsFromSettings();
    check(shortcuts::ngShortcutFor(deleteCommand).view() == "Ctrl+Alt+N",
        "shortcut load should restore command binding from settings");
    clearAllShortcuts();
}

// Complete existing regression.d19320-19346.
void testViewportPaletteCommandDiscovery() {
    using nijigenerate::compat::RetainedString;
    namespace palette = nijigenerate::commands::viewport::palette;
    namespace files = nijigenerate::commands::puppet::file;
    namespace nodes = nijigenerate::commands::node::node;
    const auto commandAt = [](const auto& table, auto key, std::size_t line) {
        const auto found = table.find(key);
        if (found == table.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return found->second;
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    auto all = palette::collectAllCommands();
    check(all.size() > 50, "command palette should collect registered commands");
    bool sawSave = false, sawNode = false;
    for (const auto command : all) {
        if (command == commandAt(files::commands, files::FileCommand::SaveFile, 19327)) sawSave = true;
        if (command == commandAt(nodes::commands, nodes::NodeCommand::AddNode, 19329)) sawNode = true;
    }
    check(sawSave, "command palette should include SaveFile command");
    check(sawNode, "command palette should include AddNode command");
    auto saveFiltered = palette::filterCommands(RetainedString::fromStaticLiteral("save"),
        commandAt(palette::commands, palette::PaletteCommand::ListCommand, 19335));
    bool filteredSave = false;
    for (const auto command : saveFiltered) {
        if (command == commandAt(files::commands, files::FileCommand::SaveFile, 19338)) filteredSave = true;
    }
    check(filteredSave, "command palette filter should find SaveFile by text");
    check(!palette::getParentCategory(commandAt(files::commands, files::FileCommand::SaveFile, 19342)).empty(),
        "command palette should derive a parent category");
    check(palette::deriveEnglishToken(commandAt(files::commands, files::FileCommand::SaveFile, 19344))
            .view().find("save") != std::string_view::npos,
        "command palette should derive an English search token");
}
