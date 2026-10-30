#include "defaults.hpp"
#include "base.hpp"
#include <nijigenerate/core/input_shortcut_template.hpp>
#include <nijigenerate/commands/puppet/file.hpp>
#include <nijigenerate/commands/puppet/edit_table.hpp>
#include <nijigenerate/commands/node/node_table.hpp>
#include <nijigenerate/commands/viewport/control_table.hpp>
#include <nijigenerate/commands/viewport/palette_table.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <cstddef>

namespace nijigenerate::core::shortcut {
namespace {
template<input::detail::ShortcutLiteral Spec>
compat::RetainedString shortcutText() noexcept {
    constexpr auto length = input::ShortcutSpec<Spec>::value.length;
    struct StaticText { char bytes[length + 1]{}; };
    static constexpr auto text = [] {
        StaticText result;
        constexpr auto& formatted = input::ShortcutSpec<Spec>::value;
        for (std::size_t i = 0; i < formatted.length; ++i) result.bytes[i] = formatted.bytes[i];
        return result;
    }();
    return compat::RetainedString::fromStaticLiteral(text.bytes);
}

template<class Table, class Key>
std::shared_ptr<commands::Command> commandAt(const Table& table, Key key, std::size_t sourceLine) {
    const auto found = table.find(key);
    if (found == table.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/shortcut/defaults.d"), sourceLine);
    return found->second;
}
}

void ngRegisterDefaultShortcuts() {
    using namespace commands;
    ngRegisterShortcut(shortcutText<"Ctrl-N">(),
        commandAt(puppet::file::commands, puppet::file::FileCommand::NewFile, 17));
    ngRegisterShortcut(shortcutText<"Ctrl-O">(),
        commandAt(puppet::file::commands, puppet::file::FileCommand::ShowOpenFileDialog, 18));
    ngRegisterShortcut(shortcutText<"Ctrl-S">(),
        commandAt(puppet::file::commands, puppet::file::FileCommand::ShowSaveFileDialog, 19));
    ngRegisterShortcut(shortcutText<"Ctrl-Shift-S">(),
        commandAt(puppet::file::commands, puppet::file::FileCommand::ShowSaveFileAsDialog, 20));
    ngRegisterShortcut(shortcutText<"Ctrl-Shift-Z">(),
        commandAt(puppet::edit::commands, puppet::edit::EditCommand::Redo, 23), true);
    ngRegisterShortcut(shortcutText<"Ctrl-Z">(),
        commandAt(puppet::edit::commands, puppet::edit::EditCommand::Undo, 24), true);
    ngRegisterShortcut(shortcutText<"Ctrl-X">(),
        commandAt(node::node::commands, node::node::NodeCommand::CutNode, 27), true);
    ngRegisterShortcut(shortcutText<"Ctrl-C">(),
        commandAt(node::node::commands, node::node::NodeCommand::CopyNode, 28), true);
    ngRegisterShortcut(shortcutText<"Ctrl-V">(),
        commandAt(node::node::commands, node::node::NodeCommand::PasteNode, 29), true);
    ngRegisterShortcut(shortcutText<"Ctrl-Shift-P">(),
        commandAt(viewport::palette::commands, viewport::palette::PaletteCommand::ListCommand, 34));
}
}
