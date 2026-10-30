// Existing regression.d21742-21777; include inside regression::detail after readPotMsgids.
void testI18nPotTemplateCoversSimpleSourceLiterals() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    namespace paths = nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const std::array potSegments{regressionRepoRoot(), text("tl"), text("template.pot")};
    auto potPath = paths::ngBuildPath(potSegments);
    require(paths::ngFileExists(potPath.view()) && paths::ngIsFile(potPath.view()), text("tl/template.pot should exist"));
    auto msgids = readPotMsgids(potPath);
    const Slice<RetainedString> requiredMsgids{
        text("Depth Bone"), text("Preview Depth Bone Deform"), text("Grid Deformer"), text("Path Deformer"),
        text("AutoMesh/Set Simple"), text("Command Browser"), text("Mask Sources"), text("Reorder Mask Source"),
        text("Set SimplePhysics Parameter"), text("Generate Mipmaps..."), text(" Apply"), text(" Approve")};
    for (const auto msgid : requiredMsgids) {
        const bool found = msgids.contains(msgid.view());
        require(found, concatenateSourceStrings({text("tl/template.pot should include msgid: "), msgid}));
    }
    const std::array localeSegments{regressionRepoRoot(), text("tl")};
    for (auto entry : paths::ngDirEntries(paths::ngBuildPath(localeSegments), paths::SpanMode::shallow)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".po")) continue;
        auto catalog = paths::ngReadUtf8Text(entry.name());
        const Slice<char32_t> oldIconChars{static_cast<char32_t>(0xE5CA)};
        // The original fixed BMP scalar selects std.utf.encode's three-byte branch.
        nicxlive::core::common::SharedArray<char> encoded;
        for (const auto scalar : oldIconChars) {
            encoded.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
            encoded.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
            encoded.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
        }
        const auto oldIcon = RetainedString::retainedView(encoded.retentionOwner(), encoded.data(), encoded.size());
        const bool valid = catalog.view().find("msgid \" Apply\"") != std::string_view::npos &&
            catalog.view().find("msgid \" Approve\"") != std::string_view::npos &&
            catalog.view().find(concatenateSourceStrings({text("msgid \""), oldIcon, text(" Apply\"")}).view()) == std::string_view::npos &&
            catalog.view().find(concatenateSourceStrings({text("msgid \""), oldIcon, text(" Approve\"")}).view()) == std::string_view::npos;
        require(valid, concatenateSourceStrings({text("translation catalog must use the current icon-prefixed message ids: "),
            entry.name().retainedString()}));
    }
}
