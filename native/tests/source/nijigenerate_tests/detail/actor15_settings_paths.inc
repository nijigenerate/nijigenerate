// Complete existing regression.d19498-19589 only.
void testSettingsPathResolution() {
    using Text = nijigenerate::compat::RetainedString;
    using Cursor = nijigenerate::compat::phobos::detail::UnicodeCursor<Text, char>;
    using Strings = nijigenerate::compat::Slice<Text>;
    namespace paths = nijigenerate::compat::phobos;
    namespace appPaths = nijigenerate::core::path;
    namespace settings = nijigenerate::core::settings;
    namespace dpi = nijigenerate::core::dpi;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    auto configPath = appPaths::ngGetAppConfigPath();
    check(!configPath.empty(), "config path should resolve");
    check(configPath.view().find(appPaths::APP_FOLDER_NAME) != std::string_view::npos,
        "config path should include application folder name");
    check(paths::ngFileExists(configPath.view()), "config path should be created");
    auto imguiPath = appPaths::ngGetAppImguiConfigFile();
    check(imguiPath.view().ends_with("imgui.ini"), "imgui config path should point to imgui.ini");
    check(imguiPath.view().find(configPath.view()) != std::string_view::npos,
        "imgui config path should live under config path");
    auto fontsPath = appPaths::ngGetAppFontsPath();
    auto localePath = appPaths::ngGetAppLocalePath();
    check(paths::ngFileExists(fontsPath.view()) && nijigenerate::platform::ngAutosaveBaseName(fontsPath.view()) == "fonts",
        "fonts path should be created under config path");
    check(paths::ngFileExists(localePath.view()) && nijigenerate::platform::ngAutosaveBaseName(localePath.view()) == "i18n",
        "locale path should be created under config path");
    auto extraLocale = appPaths::ngGetAppLocalePathExtra();
    check(extraLocale.isNull() || !extraLocale.empty(), "extra locale path should be null or non-empty");
    settings::incSettingsLoad();
    check(settings::incSettingsPath().ends_with("settings.json"), "settings path should point to settings.json");
    check(settings::incSettingsCanGet(text("ViewportZoomMode")), "default settings should include viewport zoom mode");
    check(!settings::incSettingsGet<settings::SettingsString>(text("ViewportZoomMode")).empty(),
        "default viewport zoom mode should be readable");
    dpi::incSetUIScale(1.5f);
    dpi::incInitDPIScaling();
#if defined(NG_OSX)
    check(near(dpi::incGetUIScale(), 1.0f), "OSX UI scale should remain OS-native");
#else
    check(near(dpi::incGetUIScale(), 1.5f), "UI scale should persist through settings");
#endif
    check(dpi::incGetUIScaleFont() > 0, "UI font scale should be positive");
    check(!dpi::incGetUIScaleText().empty(), "UI scale text should render");
    auto fontSource = paths::ngReadUtf8Text(text("source/nijigenerate/core/font.d"));
    check(fontSource.view().find("import(\"MaterialSymbolsOutlined.ttf\")") != std::string_view::npos,
        "the UI must embed the current Material Symbols font");
    check(fontSource.view().find("import(\"MaterialIcons.ttf\")") == std::string_view::npos &&
        fontSource.view().find("_incMaterialSymbolsFontData") == std::string_view::npos,
        "the UI must not retain the legacy Material Icons or optional runtime font fallback");
    const std::array currentFontSegments{text("res"), text("MaterialSymbolsOutlined.ttf")};
    const std::array legacyFontSegments{text("res"), text("MaterialIcons.ttf")};
    check(paths::ngFileExists(paths::ngBuildPath(currentFontSegments).view()) &&
        !paths::ngFileExists(paths::ngBuildPath(legacyFontSegments).view()),
        "the repository must contain only the current Material Symbols icon font asset");
    const auto parseHexCodepoint = [](Text value) {
        std::uint32_t result = 0;
        for (auto ch : value.view()) {
            std::uint32_t digit;
            if (ch >= '0' && ch <= '9') digit = static_cast<std::uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') digit = static_cast<std::uint32_t>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') digit = static_cast<std::uint32_t>(ch - 'A' + 10);
            else return std::numeric_limits<std::uint32_t>::max();
            result = result * 16 + digit;
        }
        return result;
    };
    // std.array.split2011-2040: decode dchars, retain each original word slice.
    const auto splitFields = [](Text line) {
        const auto stripped = nijigenerate::compat::ngStripUnicode(line.view());
        const auto first = line.empty() ? 0 : static_cast<std::size_t>(stripped.data() - line.data());
        auto input = line.subslice(first, first + stripped.size());
        Strings result;
        bool inword = false;
        std::size_t start = 0, offset = 0;
        for (Cursor cursor(input); !cursor.empty(); cursor.popFront()) {
            const auto scalar = cursor.front();
            const std::size_t width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
            const bool white = nijigenerate::compat::ngStripUnicode(input.view().substr(offset, width)).empty();
            if (white) {
                if (inword) { result.append(input.subslice(start, offset)); inword = false; }
            } else if (!inword) { start = offset; inword = true; }
            offset += width;
        }
        if (inword) result.append(input.subslice(start, input.size()));
        return result;
    };
    nicxlive::core::common::ResourceKeyMap<std::uint32_t, bool> materialSymbolCodepoints;
    const std::array codepointSegments{text("res"), text("MaterialSymbolsOutlined.codepoints")};
    for (auto line : splitSourceLines(paths::ngReadUtf8Text(paths::ngBuildPath(codepointSegments)))) {
        auto fields = splitFields(line);
        if (fields.size() < 2) continue;
        auto codepoint = parseHexCodepoint(fields[fields.size() - 1]);
        if (codepoint != std::numeric_limits<std::uint32_t>::max()) materialSymbolCodepoints[codepoint] = true;
    }
    const auto requireSupportedIconCodepoint = [&](std::uint32_t codepoint, Text sourcePath) {
        if (codepoint < 0xE000 || codepoint > 0xF8FF) return;
        const bool supported = materialSymbolCodepoints.find(codepoint) != materialSymbolCodepoints.end();
        const std::array<format::TypedFormatArgument, 2> arguments{
            format::ngFormatArgument(codepoint), format::ngFormatArgument(sourcePath)};
        auto rendered = format::ngFormatTyped(
            "UI source references a private-use glyph missing from Material Symbols: U+%04X in %s", arguments);
        auto message = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
        require(supported, message);
    };
    for (auto entry : paths::ngDirEntries(text("source"), paths::SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        auto sourceText = paths::ngReadUtf8Text(entry.name());
        for (Cursor cursor(sourceText); !cursor.empty(); cursor.popFront()) {
            requireSupportedIconCodepoint(static_cast<std::uint32_t>(cursor.front()), entry.name().retainedString());
        }
        for (std::size_t i = 0; i < sourceText.size(); ++i) {
            if (sourceText.view()[i] != '\\' || i + 5 >= sourceText.size()) continue;
            std::size_t digits;
            if (sourceText.view()[i + 1] == 'u') digits = 4;
            else if (sourceText.view()[i + 1] == 'U') digits = 8;
            else continue;
            if (i + 2 + digits > sourceText.size()) continue;
            auto codepoint = parseHexCodepoint(sourceText.subslice(i + 2, i + 2 + digits));
            if (codepoint != std::numeric_limits<std::uint32_t>::max())
                requireSupportedIconCodepoint(codepoint, entry.name().retainedString());
        }
    }
    settings::incSettingsSet(text("RegressionPathProbe"), text("ok"));
    check(settings::incSettingsGet<settings::SettingsString>(text("RegressionPathProbe")).view() == "ok",
        "settings set/get should round-trip string values");
}
