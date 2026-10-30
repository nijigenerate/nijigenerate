#pragma once
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>

#include "palette_types.hpp"
#include "palette_table.hpp"
#include "palette_command.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <concepts>

namespace nijigenerate::commands::viewport::palette {

void ngInitPaletteCommands();
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
template<auto Id, class C>
using Step = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Step<PaletteCommand::ListCommand, ListCommandCommand>>;
}

template<class T> requires (std::same_as<T, PaletteCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitPaletteCommands(); }

compat::Slice<std::shared_ptr<Command>> collectAllCommands();
compat::RetainedString getParentCategory(std::shared_ptr<Command> command);
compat::RetainedString deriveEnglishToken(std::shared_ptr<Command> command);
compat::Slice<std::shared_ptr<Command>> filterCommands(
    compat::RetainedString query, std::shared_ptr<Command> exclude = {});

} // namespace nijigenerate::commands::viewport::palette

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<viewport::palette::PaletteCommand> {
    static consteval bool supported() {
        return viewport::palette::initialization::Program::supported<viewport::palette::initialization::Table>();
    }
    template<class ActualKey = viewport::palette::PaletteCommand>
        requires (std::same_as<ActualKey, viewport::palette::PaletteCommand> && supported())
    static void invoke() { viewport::palette::ngInitCommands<ActualKey>(); }
};
}
