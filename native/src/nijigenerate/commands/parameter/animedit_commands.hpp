#pragma once
#include "animedit.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/commands/additional_tables.hpp>
namespace nijigenerate::commands::parameter::animedit {
using CommandTable = ::nijigenerate::commands::CommandTableStorage<AnimeditCommand>;
class AddAnimationKeyFrameCommand : public ExCommand<>,
    public std::enable_shared_from_this<AddAnimationKeyFrameCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit AddAnimationKeyFrameCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddAnimationKeyFrameCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    AddAnimationKeyFrameCommand();
    AddAnimationKeyFrameCommand(const AddAnimationKeyFrameCommand&) = delete;
    AddAnimationKeyFrameCommand& operator=(const AddAnimationKeyFrameCommand&) = delete;
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class CopyAnimationKeyFrameCommand : public ExCommand<>,
    public std::enable_shared_from_this<CopyAnimationKeyFrameCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit CopyAnimationKeyFrameCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CopyAnimationKeyFrameCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    CopyAnimationKeyFrameCommand();
    CopyAnimationKeyFrameCommand(const CopyAnimationKeyFrameCommand&) = delete;
    CopyAnimationKeyFrameCommand& operator=(const CopyAnimationKeyFrameCommand&) = delete;
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class PasteAnimationKeyFrameCommand : public ExCommand<>,
    public std::enable_shared_from_this<PasteAnimationKeyFrameCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit PasteAnimationKeyFrameCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<PasteAnimationKeyFrameCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    PasteAnimationKeyFrameCommand();
    PasteAnimationKeyFrameCommand(const PasteAnimationKeyFrameCommand&) = delete;
    PasteAnimationKeyFrameCommand& operator=(const PasteAnimationKeyFrameCommand&) = delete;
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
void ngInitAnimeditCommands();
}

namespace nicxlive::core::d_runtime {
#define NG_ANIMEDIT_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::parameter::animedit::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.parameter.animedit." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.parameter.animedit." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::animedit::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.animedit." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.animedit." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_ANIMEDIT_COMMAND_SOURCE_NAMES(AddAnimationKeyFrameCommand)
NG_ANIMEDIT_COMMAND_SOURCE_NAMES(CopyAnimationKeyFrameCommand)
NG_ANIMEDIT_COMMAND_SOURCE_NAMES(PasteAnimationKeyFrameCommand)
#undef NG_ANIMEDIT_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {
template<> struct CommandTraits<parameter::animedit::AddAnimationKeyFrameCommand> {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<parameter::animedit::CopyAnimationKeyFrameCommand> {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<parameter::animedit::PasteAnimationKeyFrameCommand> {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>,Command>;
};

#define NG_ANIMEDIT_CONSTRUCTOR(Type) \
template<> struct SourceCommandConstructor<parameter::animedit::Type> { \
    template<class... Args> struct Program { \
        using C = parameter::animedit::Type; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_ANIMEDIT_CONSTRUCTOR(AddAnimationKeyFrameCommand)
NG_ANIMEDIT_CONSTRUCTOR(CopyAnimationKeyFrameCommand)
NG_ANIMEDIT_CONSTRUCTOR(PasteAnimationKeyFrameCommand)
#undef NG_ANIMEDIT_CONSTRUCTOR
}

namespace nijigenerate::commands::parameter::animedit {
namespace detail {
struct InitializationBinding {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
using Initialization = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    ::nijigenerate::commands::detail::OptionalCommandRegistration<AnimeditCommand::AddAnimationKeyFrame, AddAnimationKeyFrameCommand>,
    ::nijigenerate::commands::detail::OptionalCommandRegistration<AnimeditCommand::CopyAnimationKeyFrame, CopyAnimationKeyFrameCommand>,
    ::nijigenerate::commands::detail::OptionalCommandRegistration<AnimeditCommand::PasteAnimationKeyFrame, PasteAnimationKeyFrameCommand>>;
}
template<class T> requires (std::same_as<T, AnimeditCommand> &&
    detail::Initialization::supported<detail::InitializationBinding>())
void ngInitCommands() { ngInitAnimeditCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<parameter::animedit::AnimeditCommand> {
    static consteval bool supported() {
        return parameter::animedit::detail::Initialization::supported<parameter::animedit::detail::InitializationBinding>();
    }
    template<class ActualKey = parameter::animedit::AnimeditCommand>
        requires (std::same_as<ActualKey, parameter::animedit::AnimeditCommand> && supported())
    static void invoke() { parameter::animedit::ngInitCommands<ActualKey>(); }
};
}
