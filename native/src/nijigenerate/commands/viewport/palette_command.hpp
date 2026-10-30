#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::viewport::palette { class ListCommandCommand; }
namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::viewport::palette::ListCommandCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.viewport.palette.ListCommandCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ListCommandCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.viewport.palette.ListCommandCommand";
    inline static constexpr char unqualifiedElement[] = "ListCommandCommand";
};
template<> struct SourceObjectTypeNames<::nijigenerate::commands::viewport::palette::ListCommandCommand> {
    inline static constexpr char className[] = "nijigenerate.commands.viewport.palette.ListCommandCommand";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.viewport.palette.ListCommandCommand)";
};
}

namespace nijigenerate::commands::viewport::palette {

class ListCommandCommand : public ExCommand<>, public std::enable_shared_from_this<ListCommandCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        static SourceConstructorFrame capture();
    };
    explicit ListCommandCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ListCommandCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ListCommandCommand();
    ListCommandCommand(const ListCommandCommand&) = delete;
    ListCommandCommand& operator=(const ListCommandCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;

private:
    void executeCommand(std::shared_ptr<Command> command);
};

} // namespace nijigenerate::commands::viewport::palette

namespace nijigenerate::commands {

template<> struct CommandTraits<viewport::palette::ListCommandCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiPopup};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

template<> struct SourceCommandConstructor<viewport::palette::ListCommandCommand> {
    template<class... Args> struct Program {
        using C = viewport::palette::ListCommandCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::descriptionSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands
