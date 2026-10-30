#include "animedit_commands.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <limits>
namespace nijigenerate::commands::parameter::animedit {
using namespace project;
using namespace core::actionstack;
using core::translation::ngTranslateRetained;
namespace {
struct AnimationKeyframeClipboardEntry {
    std::int32_t axis = 0;
    float value = std::numeric_limits<float>::quiet_NaN();
};
thread_local compat::Slice<AnimationKeyframeClipboardEntry> animationKeyframeClipboard;
bool copyAnimationKeyframesAtCurrentFrame(std::shared_ptr<Parameter> parameter) {
    auto playback = incAnimationGet();
    if (!playback || !playback->animation() || !parameter) return false;
    compat::Slice<AnimationKeyframeClipboardEntry> copied;
    auto lanes = compat::ngRequireObject(playback->animation())->lanes;
    for (auto& lane : lanes) {
        if (!nicxlive::core::ngResourceObjectEquals(
                compat::ngRequireObject(lane.paramRef)->targetParam, parameter)) continue;
        const auto frames = lane.frames;
        for (auto frame : frames) if (frame.frame == playback->frame()) {
            copied.append(AnimationKeyframeClipboardEntry{
                compat::ngRequireObject(lane.paramRef)->targetAxis, frame.value});
            break;
        }
    }
    if (copied.empty()) return false;
    animationKeyframeClipboard = copied; return true;
}
bool pasteAnimationKeyframesAtCurrentFrame(std::shared_ptr<Parameter> parameter) {
    if (animationKeyframeClipboard.empty() || !incAnimationGet() || !parameter) return false;
    bool changed = false;
    incActionPushGroup();
    compat::ngRunWithScopeExit([&] {
        const auto entries = animationKeyframeClipboard;
        for (auto entry : entries) {
            if (entry.axis < 0 || entry.axis >= 2) continue;
            incAnimationKeyframeAdd(parameter, entry.axis, entry.value);
            changed = true;
        }
    }, [] { incActionPopGroup(); });
    return changed;
}
std::shared_ptr<Parameter> parameterAt(std::shared_ptr<Context> context, std::size_t line) {
    const auto parameters = compat::ngRequireObject(context)->parameters();
    if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/animedit.d"), line);
    return parameters[0];
}
bool canEdit(std::shared_ptr<Context> context, std::size_t line) {
    if (incEditMode() != EditMode::AnimEdit || !incAnimationGet()) return false;
    const auto actual = compat::ngRequireObject(context);
    return actual->hasParameters() && !actual->parameters().empty() && parameterAt(actual, line);
}
}
AddAnimationKeyFrameCommand::AddAnimationKeyFrameCommand()
    : AddAnimationKeyFrameCommand(SourceConstructorFrame::capture()) {}
AddAnimationKeyFrameCommand::SourceConstructorFrame AddAnimationKeyFrameCommand::SourceConstructorFrame::capture() {
    return {
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Animation: Add Keyframe")),
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Add a keyframe for the selected parameter in the active animation. Requires Animation Edit mode and an active animation."))};
}
AddAnimationKeyFrameCommand::AddAnimationKeyFrameCommand(SourceConstructorFrame frame)
    : ExCommand(std::move(frame.label), std::move(frame.description)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AddAnimationKeyFrameCommand),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.commands.parameter.animedit.AddAnimationKeyFrameCommand"));
}
void AddAnimationKeyFrameCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddAnimationKeyFrameCommand>();
}
bool AddAnimationKeyFrameCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return canEdit(context,70);
}
std::shared_ptr<CommandResult> AddAnimationKeyFrameCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!runnable(context)) {
        if (incEditMode() != EditMode::AnimEdit) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Animation Edit mode is required"));
        if (!incAnimationGet()) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("No active animation"));
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameter"));
    }
    if (!incAnimationGet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No active animation"));
    if (compat::ngRequireObject(context)->hasParameters() && !context->parameters().empty()) {
        auto parameter = parameterAt(context,87);
        if (!parameter) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("No parameter"));
        if (parameter->isVec2) {
            incActionPushGroup();
            compat::ngRunWithScopeExit([&] {
                incAnimationKeyframeAdd(parameter, 0, compat::ngRequireObject(parameter)->value.x);
                incAnimationKeyframeAdd(parameter, 1, compat::ngRequireObject(parameter)->value.y);
            }, [] { incActionPopGroup(); });
        } else incAnimationKeyframeAdd(parameter,0,parameter->value.x);
        return CommandResult::opCall(true);
    }
    return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
}
CopyAnimationKeyFrameCommand::CopyAnimationKeyFrameCommand()
    : CopyAnimationKeyFrameCommand(SourceConstructorFrame::capture()) {}
CopyAnimationKeyFrameCommand::SourceConstructorFrame CopyAnimationKeyFrameCommand::SourceConstructorFrame::capture() {
    return {
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Animation: Copy Keyframe")),
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Copy the selected parameter's keyframe at the current animation frame."))};
}
CopyAnimationKeyFrameCommand::CopyAnimationKeyFrameCommand(SourceConstructorFrame frame)
    : ExCommand(std::move(frame.label), std::move(frame.description)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CopyAnimationKeyFrameCommand),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.commands.parameter.animedit.CopyAnimationKeyFrameCommand"));
}
void CopyAnimationKeyFrameCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<CopyAnimationKeyFrameCommand>();
}
bool CopyAnimationKeyFrameCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return canEdit(context,121);
}
std::shared_ptr<CommandResult> CopyAnimationKeyFrameCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!runnable(context)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Animation Edit mode, an active animation, and a parameter are required"));
    return copyAnimationKeyframesAtCurrentFrame(parameterAt(context,127)) ? CommandResult::opCall(true) :
        CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No keyframe at current frame"));
}
PasteAnimationKeyFrameCommand::PasteAnimationKeyFrameCommand()
    : PasteAnimationKeyFrameCommand(SourceConstructorFrame::capture()) {}
PasteAnimationKeyFrameCommand::SourceConstructorFrame PasteAnimationKeyFrameCommand::SourceConstructorFrame::capture() {
    return {
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Animation: Paste Keyframe")),
        ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "Paste copied animation keyframe values to the selected parameter at the current animation frame."))};
}
PasteAnimationKeyFrameCommand::PasteAnimationKeyFrameCommand(SourceConstructorFrame frame)
    : ExCommand(std::move(frame.label), std::move(frame.description)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PasteAnimationKeyFrameCommand),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.commands.parameter.animedit.PasteAnimationKeyFrameCommand"));
}
void PasteAnimationKeyFrameCommand::prepareSourceClassInfo() {
    ExCommand<>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<PasteAnimationKeyFrameCommand>();
}
bool PasteAnimationKeyFrameCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return canEdit(context,147) && !animationKeyframeClipboard.empty();
}
std::shared_ptr<CommandResult> PasteAnimationKeyFrameCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!runnable(context)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Animation Edit mode, an active animation, a parameter, and copied keyframes are required"));
    return pasteAnimationKeyframesAtCurrentFrame(parameterAt(context,154)) ? CommandResult::opCall(true) :
        CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No compatible copied keyframes"));
}
void ngInitAnimeditCommands() {
    detail::Initialization::run<detail::InitializationBinding>();
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::parameter::animedit::AnimationKeyframeClipboardEntry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.parameter.animedit.AnimationKeyframeClipboardEntry)";
    inline static constexpr char unqualifiedArgument[] = "(AnimationKeyframeClipboardEntry)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.parameter.animedit.AnimationKeyframeClipboardEntry";
    inline static constexpr char unqualifiedElement[] = "AnimationKeyframeClipboardEntry";
};
}
