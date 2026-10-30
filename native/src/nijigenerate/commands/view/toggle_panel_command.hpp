#pragma once
#include "nijigenerate/commands/command.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <memory>

namespace nijigenerate::panels { class Panel; }

namespace nijigenerate::commands::view::panel {
struct TogglePanelArguments {
    std::shared_ptr<::nijigenerate::panels::Panel> panel;
};
}

namespace nijigenerate::commands {
template<> struct CommandFields<view::panel::TogglePanelArguments> {
    using Storage = view::panel::TogglePanelArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::panel, "panel", "target panel to toggle", "nijigenerate.panels.Panel">{}
    };
};
}

namespace nijigenerate::commands::view::panel {
class TogglePanelVisibilityCommand : public ExCommand<TogglePanelArguments>,
    public std::enable_shared_from_this<TogglePanelVisibilityCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        static SourceConstructorFrame capture();
    };
    explicit TogglePanelVisibilityCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<TogglePanelVisibilityCommand>;
public:
    TogglePanelVisibilityCommand();
    TogglePanelVisibilityCommand(const TogglePanelVisibilityCommand&) = delete;
    TogglePanelVisibilityCommand& operator=(const TogglePanelVisibilityCommand&) = delete;
    compat::RetainedString label() override;
    compat::RetainedString description() override;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
}

namespace nijigenerate::commands {
template<> struct SourceCommandConstructor<view::panel::TogglePanelVisibilityCommand> {
    template<class... Args> struct Program {
        using C = view::panel::TogglePanelVisibilityCommand;
        using Storage = view::panel::TogglePanelArguments;
        using Panel = std::shared_ptr<::nijigenerate::panels::Panel>;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>;
            } && SourceExCommandConstructorProgram<Storage>::template preferredSupported<Panel>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct CommandTraits<view::panel::TogglePanelVisibilityCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<view::panel::TogglePanelArguments>>;
};
}
