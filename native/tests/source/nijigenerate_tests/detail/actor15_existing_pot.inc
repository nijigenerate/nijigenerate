// Complete existing regression.d21742-21777, same module-private namespace.
void testI18nPotTemplateCoversSimpleSourceLiterals() {
    using Text = nijigenerate::compat::RetainedString;
    namespace paths = nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const std::array potSegments{regressionRepoRoot(), text("tl"), text("template.pot")};
    auto potPath = paths::ngBuildPath(potSegments);
    require(paths::ngFileExists(potPath.view()) && paths::ngIsFile(potPath.view()), text("tl/template.pot should exist"));
    auto msgids = readPotMsgids(potPath);
    nijigenerate::compat::Slice<Text> requiredMsgids{
        text("Depth Bone"), text("Preview Depth Bone Deform"), text("Grid Deformer"), text("Path Deformer"),
        text("AutoMesh/Set Simple"), text("Command Browser"), text("Mask Sources"), text("Reorder Mask Source"),
        text("Set SimplePhysics Parameter"), text("Generate Mipmaps..."), text(" Apply"), text(" Approve")};
    for (const auto msgid : requiredMsgids) {
        const bool present = msgids.find(msgid.view()) != nullptr;
        const auto message = concatenateSourceStrings({text("tl/template.pot should include msgid: "), msgid});
        require(present, message);
    }
    const std::array catalogSegments{regressionRepoRoot(), text("tl")};
    for (auto entry : paths::ngDirEntries(paths::ngBuildPath(catalogSegments), paths::SpanMode::shallow)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".po")) continue;
        auto catalog = paths::ngReadUtf8Text(entry.name());
        nijigenerate::compat::Slice<char32_t> oldIconChars{static_cast<char32_t>(0xE5CA)};
        // This is the original single valid U+E5CA input to toUTFImpl!string:
        // reserve its length, then append its actual byUTF!char code units.
        // No generic Unicode conversion interface or substituted glyph is added.
        std::string encoded;
        nijigenerate::platform::detail::ngPathAllocation([&] { encoded.reserve(oldIconChars.size()); });
        const auto scalar = static_cast<std::uint32_t>(oldIconChars[0]);
        const auto put = [&](std::uint32_t byte) {
            nijigenerate::platform::detail::ngPathAllocation([&] {
                encoded.push_back(std::bit_cast<char>(static_cast<std::uint8_t>(byte)));
            });
        };
        put(0xE0 | (scalar >> 12));
        put(0x80 | ((scalar >> 6) & 0x3F));
        put(0x80 | (scalar & 0x3F));
        auto oldIcon = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(encoded)); });
        const bool current = catalog.view().find("msgid \" Apply\"") != std::string_view::npos &&
            catalog.view().find("msgid \" Approve\"") != std::string_view::npos &&
            catalog.view().find(concatenateSourceStrings({text("msgid \""), oldIcon, text(" Apply\"")}).view()) == std::string_view::npos &&
            catalog.view().find(concatenateSourceStrings({text("msgid \""), oldIcon, text(" Approve\"")}).view()) == std::string_view::npos;
        const auto message = concatenateSourceStrings({text("translation catalog must use the current icon-prefixed message ids: "),
            entry.name().retainedString()});
        require(current, message);
    }
}
