#pragma once
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/widgets/output.hpp>
#include <nijigenerate/widgets/inputtext.hpp>

namespace nijigenerate::panels::shell {
class Command : public std::enable_shared_from_this<Command> {
public:
    compat::RetainedString command;
    widgets::output::OutputPtr output;
    Command(compat::RetainedString text, widgets::output::OutputPtr result);
    virtual ~Command() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Command> other);
    virtual int opCmp(std::shared_ptr<Command> other);
protected:
    virtual void prepareSourceClassInfo();
};
class ShellPanel : public Panel, public widgets::output::CommandIssuer,
        public std::enable_shared_from_this<ShellPanel> {
    compat::RetainedString command;
    compat::RetainedString nextCommand;
    bool forceUpdatePreview = false;
    compat::Slice<std::shared_ptr<Command>> history;
    widgets::output::OutputPtr latestOutput;
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShellPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.shell.shell.ShellPanel"));
            return true;
        }();
        (void)registered;
    }
    virtual void updatePreview();
    void onUpdate() override;
public:
    ShellPanel();
    void addCommand(compat::RetainedString) override;
    void setCommand(compat::RetainedString) override;
    void addPopup(core::selector::ResourcePtr) override;
};
void ngInitializeShellPanelModule();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::shell::Command> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.shell.shell.Command)";
    inline static constexpr char unqualifiedArgument[] = "(Command)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.shell.shell.Command";
    inline static constexpr char unqualifiedElement[] = "Command";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::shell::Command> {
    inline static constexpr char className[] = "nijigenerate.panels.shell.shell.Command";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.shell.shell.Command)";
};
template<> struct SourceTypeSpellings<::nijigenerate::panels::shell::ShellPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.shell.shell.ShellPanel)";
    inline static constexpr char unqualifiedArgument[] = "(ShellPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.shell.shell.ShellPanel";
    inline static constexpr char unqualifiedElement[] = "ShellPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::shell::ShellPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.shell.shell.ShellPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.shell.shell.ShellPanel)";
};
}
