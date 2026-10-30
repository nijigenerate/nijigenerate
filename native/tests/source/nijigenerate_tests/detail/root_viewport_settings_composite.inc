// Faithful bodies of regression.d12071-12109 and 12468-12500; reuse the shared shortcut helper.
// Include once outside namespaces, after the module's other helper definitions.
#include "root_viewport_settings_composite.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/node_table.hpp>
#include <nijigenerate/commands/viewport/control_pair_commands.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/node.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
std::ptrdiff_t countUntilRegressionNeedle(std::string_view line, std::string_view needle);

// Existing regression.d18825-18828, shared by the original shortcut cases.
void clearAllShortcuts() {
    auto entries = nijigenerate::core::shortcut::ngListShortcuts();
    for (auto entry : entries)
        nijigenerate::core::shortcut::ngClearShortcutFor(entry.command);
}

void testViewportSettingsCommandCompositeWorkflow() {
    using namespace nijigenerate::core::shortcut;
    using namespace nijigenerate::commands::viewport::control;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    resetCase();
    clearAllShortcuts();
    // std.conv.toChars!10(uint).array, using the real Source string allocator.
    const auto uuidText = [](std::uint32_t input) {
        std::array<char, 10> buffer;
        auto first = buffer.size();
        std::uint64_t value = input;
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto bytes = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(bytes.data(), buffer.data() + first, bytes.size());
        return RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
    };

    auto addNodeCommand = nijigenerate::commands::node::node::commands[
        nijigenerate::commands::node::node::NodeCommand::AddNode];
    require(addNodeCommand != nullptr,
        RetainedString::fromStaticLiteral("viewport/settings composite requires AddNode command"));
    ngRegisterShortcut(RetainedString::fromStaticLiteral("Ctrl+Alt+Shift+N"), addNodeCommand);
    ngSaveShortcutsToSettings();

    auto puppet = nijigenerate::project::incActivePuppet();
    auto left = Node::createSourceDeferred([&] { return puppet->root; });
    auto right = Node::createSourceDeferred([&] { return puppet->root; });
    left->setName(NullableString::fromStaticLiteral("CompositeHand.L"));
    right->setName(NullableString::fromStaticLiteral("CompositeHand.R"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(puppet);

    auto add = constructSourceCommand<AddFlipPairCommand>();
    add->left = left;
    add->right = right;
    require(add->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("viewport/settings composite should add flip pair"));
    {
        const auto listing = nijigenerate::compat::ngJsonStringify(
            constructSourceCommand<ListFlipPairsCommand>()->run(ctx)->result);
        const auto id = uuidText(left->getUUID());
        require(countUntilRegressionNeedle(listing.view(), id.view()) >= 0,
            RetainedString::fromStaticLiteral("viewport/settings composite should list added flip pair"));
    }

    nijigenerate::core::actionstack::incActionUndo();
    {
        const auto listing = nijigenerate::compat::ngJsonStringify(
            constructSourceCommand<ListFlipPairsCommand>()->run(ctx)->result);
        const auto id = uuidText(left->getUUID());
        require(countUntilRegressionNeedle(listing.view(), id.view()) < 0,
            RetainedString::fromStaticLiteral("undo viewport/settings composite should remove flip pair"));
    }
    require(ngShortcutFor(addNodeCommand).view() == "Ctrl+Alt+Shift+N",
        RetainedString::fromStaticLiteral("undo viewport command should not disturb persisted shortcut setting"));

    nijigenerate::core::actionstack::incActionRedo();
    {
        const auto listing = nijigenerate::compat::ngJsonStringify(
            constructSourceCommand<ListFlipPairsCommand>()->run(ctx)->result);
        const auto id = uuidText(left->getUUID());
        require(countUntilRegressionNeedle(listing.view(), id.view()) >= 0,
            RetainedString::fromStaticLiteral("redo viewport/settings composite should restore flip pair"));
    }
    clearAllShortcuts();
    ngLoadShortcutsFromSettings();
    require(ngShortcutFor(addNodeCommand).view() == "Ctrl+Alt+Shift+N",
        RetainedString::fromStaticLiteral(
            "viewport/settings composite should reload shortcut after viewport history operations"));
    clearAllShortcuts();
}
void testSettingsCompositeShortcutsRecentWorkflow() {
    using namespace nijigenerate::core::shortcut;
    using namespace nijigenerate::core::settings;
    using nijigenerate::compat::RetainedString;
    resetCase();
    incSettingsLoad();
    clearAllShortcuts();

    auto oldProjects = nijigenerate::project::incGetPrevProjects();
    nijigenerate::compat::ngRunWithScopeExit([&] {
        incSettingsSet(SettingsString(RetainedString::fromStaticLiteral("prev_projects")), SettingsStringArray{});
        auto addCommand = nijigenerate::commands::node::node::commands[
            nijigenerate::commands::node::node::NodeCommand::AddNode];
        auto deleteCommand = nijigenerate::commands::node::node::commands[
            nijigenerate::commands::node::node::NodeCommand::DeleteNode];
        require(addCommand != nullptr && deleteCommand != nullptr,
            RetainedString::fromStaticLiteral("settings composite requires node commands"));
        ngRegisterShortcut(RetainedString::fromStaticLiteral("Ctrl+Alt+M"), addCommand);
        ngRegisterShortcut(RetainedString::fromStaticLiteral("Ctrl+Alt+M"), deleteCommand);
        require(ngShortcutFor(addCommand).empty() && ngShortcutFor(deleteCommand).view() == "Ctrl+Alt+M",
            RetainedString::fromStaticLiteral("settings composite shortcut conflict should move binding"));
        ngSaveShortcutsToSettings();

        nijigenerate::project::incAddPrevProject(
            SettingsString(RetainedString::fromStaticLiteral("/tmp/settings-composite-a.inx")));
        nijigenerate::project::incAddPrevProject(
            SettingsString(RetainedString::fromStaticLiteral("/tmp/settings-composite-b.inx")));
        nijigenerate::project::incAddPrevProject(
            SettingsString(RetainedString::fromStaticLiteral("/tmp/settings-composite-a.inx")));
        auto projects = nijigenerate::project::incGetPrevProjects();
        require(projects.size() == 2 && projects[0].view().ends_with("settings-composite-a.inx"),
            RetainedString::fromStaticLiteral("settings composite recent files should promote duplicate"));

        clearAllShortcuts();
        ngLoadShortcutsFromSettings();
        require(ngShortcutFor(deleteCommand).view() == "Ctrl+Alt+M",
            RetainedString::fromStaticLiteral(
                "settings composite shortcut reload should survive recent-file mutations"));
    }, [&] {
        incSettingsSet(SettingsString(RetainedString::fromStaticLiteral("prev_projects")), oldProjects);
        clearAllShortcuts();
    });
}
} // namespace nijigenerate_tests::regression::detail
