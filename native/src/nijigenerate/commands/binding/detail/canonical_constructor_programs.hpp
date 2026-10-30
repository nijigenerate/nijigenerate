#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include "constructor_body.hpp"
#include <nijigenerate/commands/register_command.hpp>

namespace nijigenerate::commands {

// These declarations correspond to the actual constructor bodies which call
// BindingSourceConstructor<C>::description(), not a list of capability flags.
#define NG_BINDING_CANONICAL_CONSTRUCTOR(Type, BaseBody) \
template<> struct SourceCommandConstructor<binding::Type> { \
    template<class... Args> struct Program { \
        using C = binding::Type; \
        using Body = binding::detail::BindingSourceConstructor<C>; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { Body::description() } -> std::same_as<compat::RetainedString>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::BaseBody(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_BINDING_CANONICAL_CONSTRUCTOR(UnsetKeyFrameCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SetKeyFrameCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(ResetKeyFrameCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(InvertKeyFrameCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(MirrorKeyFrameHorizontallyCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(MirrorKeyFrameVerticallyCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(FlipDeformCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SymmetrizeDeformCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SetFromHorizontalMirrorCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SetFromVerticalMirrorCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SetFromDiagonalMirrorCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(SetFrom1DMirrorCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(CopyBindingCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(PasteBindingCommand, descriptionSupported)
NG_BINDING_CANONICAL_CONSTRUCTOR(RemoveBindingCommand, pairSupported)
#undef NG_BINDING_CANONICAL_CONSTRUCTOR

template<> struct SourceCommandConstructor<binding::SetInterpolationCommand> {
    template<class... Args> struct Program {
        using C = binding::SetInterpolationCommand;
        using Body = binding::detail::BindingSourceConstructor<C>;
        using Mode = typename Body::Mode;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<Mode>, Args...>() && requires(Mode mode) {
                { Body::description(mode) } -> std::same_as<compat::RetainedString>;
            } && SourceExCommandConstructorProgram<binding::SetInterpolationArguments>::preferredSupported<Mode>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Mode>>(allocator,
                std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands
