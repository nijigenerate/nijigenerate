// Existing regression.d20662-20743; include inside regression::detail after inventory helpers.
void testCoverageSourceModuleScenarioInventory() {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    Strings failures;
    auto inventory = collectSourceModuleScenarioInventory(failures);
    const Strings requiredPrefixes{
        RetainedString::fromStaticLiteral("api"), RetainedString::fromStaticLiteral("atlas"),
        RetainedString::fromStaticLiteral("automesh"), RetainedString::fromStaticLiteral("core"),
        RetainedString::fromStaticLiteral("deform"), RetainedString::fromStaticLiteral("depth"),
        RetainedString::fromStaticLiteral("depthbone"), RetainedString::fromStaticLiteral("inspectors"),
        RetainedString::fromStaticLiteral("io"), RetainedString::fromStaticLiteral("mesh"),
        RetainedString::fromStaticLiteral("node"), RetainedString::fromStaticLiteral("panels"),
        RetainedString::fromStaticLiteral("parameter"), RetainedString::fromStaticLiteral("part"),
        RetainedString::fromStaticLiteral("platform"), RetainedString::fromStaticLiteral("project"),
        RetainedString::fromStaticLiteral("render"), RetainedString::fromStaticLiteral("settings"),
        RetainedString::fromStaticLiteral("simplephysics"), RetainedString::fromStaticLiteral("undo"),
        RetainedString::fromStaticLiteral("viewport"), RetainedString::fromStaticLiteral("windows")};
    for (const auto prefix : requiredPrefixes) {
        if (!inventory.contains(prefix.view()) || inventory.get(prefix.view()).size() == 0) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "scenario family has no source module owner rows: "), prefix}));
        }
    }
    std::size_t total = 0;
    inventory.forEachRetained([&](RetainedString, Strings modules) { total += modules.size(); });
    require(total >= 265, RetainedString::fromStaticLiteral("module scenario inventory should cover the whole source tree"));
    const bool noFailures = failures.size() == 0;
    require(noFailures, concatenateSourceStrings({RetainedString::fromStaticLiteral("source module scenario inventory failures:\n"),
        joinSourceStrings({failures.data(), failures.size()}, RetainedString::fromStaticLiteral("\n"))}));
}

void testCoverageFullFeatureScenarioInventory() {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    Strings failures;
    auto inventory = collectFullFeatureScenarioInventory(failures);
    const Strings requiredKinds{
        RetainedString::fromStaticLiteral("command"), RetainedString::fromStaticLiteral("window"),
        RetainedString::fromStaticLiteral("panel"), RetainedString::fromStaticLiteral("inspector"),
        RetainedString::fromStaticLiteral("tool")};
    for (const auto kind : requiredKinds) {
        if (!inventory.contains(kind.view()) || inventory.get(kind.view()).size() == 0) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "feature inventory missing kind: "), kind}));
        }
    }
    const Strings requiredComputerUseScenarios{
        RetainedString::fromStaticLiteral("depth.edit-live-ui-smoke"),
        RetainedString::fromStaticLiteral("project.psd-depth-map-import-ui-smoke")};
    for (const auto id : requiredComputerUseScenarios) {
        if (!hasScenarioStatus(id, RetainedString::fromStaticLiteral("computer-use"))) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "missing required computer-use scenario: "), id}));
        }
    }
    require(inventory.contains("command") && inventory.get("command").size() >= 170,
        RetainedString::fromStaticLiteral("feature scenario inventory should include command classes"));
    require(inventory.contains("window") && inventory.get("window").size() >= 16,
        RetainedString::fromStaticLiteral("feature scenario inventory should include window classes"));
    require(inventory.contains("panel") && inventory.get("panel").size() >= 14,
        RetainedString::fromStaticLiteral("feature scenario inventory should include panel classes"));
    require(inventory.contains("inspector") && inventory.get("inspector").size() >= 15,
        RetainedString::fromStaticLiteral("feature scenario inventory should include inspector classes"));
    require(inventory.contains("tool") && inventory.get("tool").size() >= 13,
        RetainedString::fromStaticLiteral("feature scenario inventory should include mesh/depth tool classes"));
    std::size_t total = 0;
    inventory.forEachRetained([&](RetainedString, Strings items) { total += items.size(); });
    require(total >= 230, RetainedString::fromStaticLiteral(
        "feature scenario inventory should cover all user-facing feature entrypoint classes"));
    const bool noFailures = failures.size() == 0;
    require(noFailures, concatenateSourceStrings({RetainedString::fromStaticLiteral("feature scenario inventory failures:\n"),
        joinSourceStrings({failures.data(), failures.size()}, RetainedString::fromStaticLiteral("\n"))}));
}

