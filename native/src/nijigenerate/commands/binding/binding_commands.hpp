#pragma once

#include "binding_types.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <common/utils.hpp>
#include <memory>

namespace nicxlive::core::param { class ParameterBinding; }

namespace nijigenerate::commands::binding {

class UnsetKeyFrameCommand : public ExCommand<>, public std::enable_shared_from_this<UnsetKeyFrameCommand> {
public:
    UnsetKeyFrameCommand();
    UnsetKeyFrameCommand(const UnsetKeyFrameCommand&) = delete;
    UnsetKeyFrameCommand& operator=(const UnsetKeyFrameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SetKeyFrameCommand : public ExCommand<>, public std::enable_shared_from_this<SetKeyFrameCommand> {
public:
    SetKeyFrameCommand();
    SetKeyFrameCommand(const SetKeyFrameCommand&) = delete;
    SetKeyFrameCommand& operator=(const SetKeyFrameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class ResetKeyFrameCommand : public ExCommand<>, public std::enable_shared_from_this<ResetKeyFrameCommand> {
public:
    ResetKeyFrameCommand();
    ResetKeyFrameCommand(const ResetKeyFrameCommand&) = delete;
    ResetKeyFrameCommand& operator=(const ResetKeyFrameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class InvertKeyFrameCommand : public ExCommand<>, public std::enable_shared_from_this<InvertKeyFrameCommand> {
public:
    InvertKeyFrameCommand();
    InvertKeyFrameCommand(const InvertKeyFrameCommand&) = delete;
    InvertKeyFrameCommand& operator=(const InvertKeyFrameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class MirrorKeyFrameHorizontallyCommand : public ExCommand<>,
        public std::enable_shared_from_this<MirrorKeyFrameHorizontallyCommand> {
public:
    MirrorKeyFrameHorizontallyCommand();
    MirrorKeyFrameHorizontallyCommand(const MirrorKeyFrameHorizontallyCommand&) = delete;
    MirrorKeyFrameHorizontallyCommand& operator=(const MirrorKeyFrameHorizontallyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class MirrorKeyFrameVerticallyCommand : public ExCommand<>,
        public std::enable_shared_from_this<MirrorKeyFrameVerticallyCommand> {
public:
    MirrorKeyFrameVerticallyCommand();
    MirrorKeyFrameVerticallyCommand(const MirrorKeyFrameVerticallyCommand&) = delete;
    MirrorKeyFrameVerticallyCommand& operator=(const MirrorKeyFrameVerticallyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class FlipDeformCommand : public ExCommand<>, public std::enable_shared_from_this<FlipDeformCommand> {
public:
    FlipDeformCommand();
    FlipDeformCommand(const FlipDeformCommand&) = delete;
    FlipDeformCommand& operator=(const FlipDeformCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SymmetrizeDeformCommand : public ExCommand<>, public std::enable_shared_from_this<SymmetrizeDeformCommand> {
public:
    SymmetrizeDeformCommand();
    SymmetrizeDeformCommand(const SymmetrizeDeformCommand&) = delete;
    SymmetrizeDeformCommand& operator=(const SymmetrizeDeformCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SetFromHorizontalMirrorCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetFromHorizontalMirrorCommand> {
public:
    SetFromHorizontalMirrorCommand();
    SetFromHorizontalMirrorCommand(const SetFromHorizontalMirrorCommand&) = delete;
    SetFromHorizontalMirrorCommand& operator=(const SetFromHorizontalMirrorCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SetFromVerticalMirrorCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetFromVerticalMirrorCommand> {
public:
    SetFromVerticalMirrorCommand();
    SetFromVerticalMirrorCommand(const SetFromVerticalMirrorCommand&) = delete;
    SetFromVerticalMirrorCommand& operator=(const SetFromVerticalMirrorCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SetFromDiagonalMirrorCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetFromDiagonalMirrorCommand> {
public:
    SetFromDiagonalMirrorCommand();
    SetFromDiagonalMirrorCommand(const SetFromDiagonalMirrorCommand&) = delete;
    SetFromDiagonalMirrorCommand& operator=(const SetFromDiagonalMirrorCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class SetFrom1DMirrorCommand : public ExCommand<>, public std::enable_shared_from_this<SetFrom1DMirrorCommand> {
public:
    SetFrom1DMirrorCommand();
    SetFrom1DMirrorCommand(const SetFrom1DMirrorCommand&) = delete;
    SetFrom1DMirrorCommand& operator=(const SetFrom1DMirrorCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class CopyBindingCommand : public ExCommand<>, public std::enable_shared_from_this<CopyBindingCommand> {
public:
    CopyBindingCommand();
    CopyBindingCommand(const CopyBindingCommand&) = delete;
    CopyBindingCommand& operator=(const CopyBindingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class PasteBindingCommand : public ::nijigenerate::commands::detail::CovariantCommand<PasteBindingCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<PasteBindingCommand>;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::ParameterBinding>>;
    PasteBindingCommand();
    PasteBindingCommand(const PasteBindingCommand&) = delete;
    PasteBindingCommand& operator=(const PasteBindingCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

class RemoveBindingCommand : public ::nijigenerate::commands::detail::CovariantCommand<RemoveBindingCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<RemoveBindingCommand>;
public:
    using Result = DeleteResult<std::shared_ptr<nicxlive::core::param::ParameterBinding>>;
    RemoveBindingCommand();
    RemoveBindingCommand(const RemoveBindingCommand&) = delete;
    RemoveBindingCommand& operator=(const RemoveBindingCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

struct SetInterpolationArguments {
    nicxlive::core::common::InterpolateMode mode = nicxlive::core::common::InterpolateMode::Nearest;
};

} // namespace nijigenerate::commands::binding

namespace nijigenerate::commands {

template<> struct CommandFields<binding::SetInterpolationArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&binding::SetInterpolationArguments::mode, "mode",
            "specify the new interpolation mode.", "InterpolateMode">{}
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::binding {

class SetInterpolationCommand : public ExCommand<SetInterpolationArguments>,
        public std::enable_shared_from_this<SetInterpolationCommand> {
public:
    explicit SetInterpolationCommand(nicxlive::core::common::InterpolateMode mode);
    SetInterpolationCommand(const SetInterpolationCommand&) = delete;
    SetInterpolationCommand& operator=(const SetInterpolationCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::binding

namespace nijigenerate::commands {
namespace detail {
struct BindingKeyframeEffectTraits {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
} // namespace detail

template<> struct CommandTraits<binding::UnsetKeyFrameCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SetKeyFrameCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::ResetKeyFrameCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::InvertKeyFrameCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::MirrorKeyFrameHorizontallyCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::MirrorKeyFrameVerticallyCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::FlipDeformCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SymmetrizeDeformCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SetFromHorizontalMirrorCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SetFromVerticalMirrorCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SetFromDiagonalMirrorCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::SetFrom1DMirrorCommand> : detail::BindingKeyframeEffectTraits {};
template<> struct CommandTraits<binding::CopyBindingCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<binding::PasteBindingCommand> {
    static constexpr auto attributes = std::tuple{EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<binding::RemoveBindingCommand> {
    static constexpr auto attributes = std::tuple{EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<binding::SetInterpolationCommand> {
    static constexpr auto attributes = std::tuple{EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<binding::SetInterpolationArguments>, Command>;
};

} // namespace nijigenerate::commands

#include "detail/canonical_constructor_programs.hpp"
