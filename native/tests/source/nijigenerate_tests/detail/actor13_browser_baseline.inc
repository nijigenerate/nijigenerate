// Existing regression.d21990-22015; the original regex normalizer remains a real dependency.
nijigenerate::compat::RetainedString normalizeCommandBrowserDifferentialReport(
    nijigenerate::compat::RetainedString report);

void testCommandBrowserDifferentialReportMatchesBaseline() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using namespace nijigenerate::compat::phobos;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    nijigenerate::commands::ngInitAllCommands();
    resetCase();
    auto root = nijigenerate::project::incActivePuppet()->root;
    auto part = Part::createSourceDeferred([&] { return root; });
    part->setName(NullableString::fromStaticLiteral("DiffPart"));
    auto nodeA = Node::createSourceDeferred([&] { return root; });
    nodeA->setName(NullableString::fromStaticLiteral("DiffNodeA"));
    auto nodeB = Node::createSourceDeferred([&] { return root; });
    nodeB->setName(NullableString::fromStaticLiteral("DiffNodeB"));
    auto paramA = new2DParameter(text("DiffParamA"));
    auto paramB = new2DParameter(text("DiffParamB"));
    newValueBinding(paramA, nodeA, text("transform.t.x"), true);
    newValueBinding(paramB, nodeB, text("transform.t.y"), true);
    auto actual = normalizeCommandBrowserDifferentialReport(
        nijigenerate::windows::command_browser::ngCommandBrowserDifferentialReport());
    const std::array segments{regressionRepoRoot(), text("source"), text("nijigenerate_tests"),
        text("fixtures"), text("command_browser_differential_expected.txt")};
    auto expectedPath = ngBuildPath(segments);
    auto expectedStorage = ngReadUtf8Text(expectedPath);
    auto expected = nijigenerate::compat::ngStripUnicode(expectedStorage.view());
    if (nijigenerate::compat::ngStripUnicode(actual.view()) != expected) {
        const std::array outputSegments{text("out"), text("command_browser_differential_actual.txt")};
        auto outputPath = ngBuildPath(outputSegments);
        ngFileWrite(RetainedStringSlice(outputPath), actual.view());
    }
    require(nijigenerate::compat::ngStripUnicode(actual.view()) == expected,
        text("Command Browser applyArgs differential report changed from the 2959c149 baseline"));
}
