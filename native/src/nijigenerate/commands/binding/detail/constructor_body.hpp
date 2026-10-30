#pragma once

#include "../binding_types.hpp"
#include <nijigenerate/commands/command.hpp>

namespace nijigenerate::commands::binding {
class UnsetKeyFrameCommand;
class SetKeyFrameCommand;
class ResetKeyFrameCommand;
class InvertKeyFrameCommand;
class MirrorKeyFrameHorizontallyCommand;
class MirrorKeyFrameVerticallyCommand;
class FlipDeformCommand;
class SymmetrizeDeformCommand;
class SetFromHorizontalMirrorCommand;
class SetFromVerticalMirrorCommand;
class SetFromDiagonalMirrorCommand;
class SetFrom1DMirrorCommand;
class CopyBindingCommand;
class PasteBindingCommand;
class RemoveBindingCommand;
class SetInterpolationCommand;
}
#include "source_alias.hpp"
#include <nijigenerate/core/translation.hpp>
#include <compat/source_allocator.hpp>
#include <memory>
#include <utility>

namespace nijigenerate::commands::binding::detail {

// Intentionally incomplete for a constructor whose Source body has not been
// translated. Missing native body correspondence is not Source rejection.
template<class C> struct BindingSourceConstructor;

template<class C, CommandLiteral Description> struct BindingDescriptionConstructorBody {
    using ActualType = C;
    static compat::RetainedString description() {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(Description.value));
    }
    static std::shared_ptr<C> construct() requires requires { new C(); description(); } {
        return std::allocate_shared<C>(nicxlive::core::d_runtime::SourceAllocator<C>{});
    }
};

#define NG_BINDING_DESCRIPTION_BODY(Type, Text) \
    template<> struct BindingSourceConstructor<Type> : BindingDescriptionConstructorBody<Type, Text> {};
NG_BINDING_DESCRIPTION_BODY(UnsetKeyFrameCommand, "Unset Key Frame")
NG_BINDING_DESCRIPTION_BODY(SetKeyFrameCommand, "Set Key Frame")
NG_BINDING_DESCRIPTION_BODY(ResetKeyFrameCommand, "Reset Key Frame")
NG_BINDING_DESCRIPTION_BODY(InvertKeyFrameCommand, "Invert Key Frame")
NG_BINDING_DESCRIPTION_BODY(MirrorKeyFrameHorizontallyCommand, "Mirror Key Frame Horizontally")
NG_BINDING_DESCRIPTION_BODY(MirrorKeyFrameVerticallyCommand, "Mirror Key Frame Vertically")
NG_BINDING_DESCRIPTION_BODY(FlipDeformCommand, "Flip Deform")
NG_BINDING_DESCRIPTION_BODY(SymmetrizeDeformCommand, "Symmetrize Deform")
NG_BINDING_DESCRIPTION_BODY(SetFromHorizontalMirrorCommand, "Set From Horizontal Mirror")
NG_BINDING_DESCRIPTION_BODY(SetFromVerticalMirrorCommand, "Set From Vertical Mirror")
NG_BINDING_DESCRIPTION_BODY(SetFromDiagonalMirrorCommand, "Set From Diagonal Mirror")
NG_BINDING_DESCRIPTION_BODY(SetFrom1DMirrorCommand, "Set From 1D Mirror")
NG_BINDING_DESCRIPTION_BODY(CopyBindingCommand, "Copy Bindings")
NG_BINDING_DESCRIPTION_BODY(PasteBindingCommand, "Paste Bindings")
NG_BINDING_DESCRIPTION_BODY(RemoveBindingCommand, "Remove Bindings")
#undef NG_BINDING_DESCRIPTION_BODY

template<> struct BindingSourceConstructor<SetInterpolationCommand> {
    using ActualType = SetInterpolationCommand;
    using Mode = nicxlive::core::common::InterpolateMode;
    static compat::RetainedString description(Mode mode);
    static std::shared_ptr<SetInterpolationCommand> construct(Mode mode);
};

// BindingCommand is a real visible allocation type, independently of the
// sixteen command classes. Its original first enumerator is zero.
template<> struct BindingSourceConstructor<BindingCommand> {
    using ActualType = BindingCommand;
    static std::shared_ptr<BindingCommand> construct() {
        return std::allocate_shared<BindingCommand>(
            nicxlive::core::d_runtime::SourceAllocator<BindingCommand>{});
    }
};

// The imported Source ExCommand is abstract. Use the actual abstract native
// type's new-expression, not an invented class or an abstractness result flag.
template<class Storage> struct BindingSourceConstructor<ExCommand<Storage>> {
    using ActualType = ExCommand<Storage>;
    template<class... Args>
    static std::shared_ptr<ExCommand<Storage>> construct(Args&&... args)
        requires requires { new ExCommand<Storage>(std::forward<Args>(args)...); } {
        return std::allocate_shared<ExCommand<Storage>>(
            nicxlive::core::d_runtime::SourceAllocator<ExCommand<Storage>>{}, std::forward<Args>(args)...);
    }
};

} // namespace nijigenerate::commands::binding::detail

// The declarations above must precede the class header's canonical records.
// Direct users of this existing header still receive the complete real classes.
#include "../binding_commands.hpp"
