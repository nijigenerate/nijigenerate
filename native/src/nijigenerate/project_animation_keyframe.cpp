#include <nijigenerate/project.hpp>
#include <nijigenerate/project_animation_detail.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
namespace nijigenerate::project {
namespace {
using Lanes = nicxlive::core::common::SharedArray<nicxlive::core::AnimationLane>;
using Animation = nicxlive::core::Animation;
Lanes cloneAnimationLanes(Lanes lanes) {
    return detail::ngCloneAnimationLanes(lanes);
}
void restoreAnimationLanes(Animation* animation, Lanes lanes) {
    detail::ngRestoreAnimationLanes(animation, lanes);
}
class AnimationKeyframeChangeAction : public actions::Action,
    public std::enable_shared_from_this<AnimationKeyframeChangeAction> {
public:
    AnimationPlaybackRef playback;
    Animation* animation = nullptr;
    Lanes oldLanes, newLanes;
    bool applied = false;
    AnimationKeyframeChangeAction() = default;
    void initialize(AnimationPlaybackRef playback, Animation* animation, Lanes oldLanes, Lanes newLanes,
        std::shared_ptr<Animation> animationRoot = {}) {
        this->playback = std::move(playback);
        this->animation = animation;
        this->oldLanes = cloneAnimationLanes(oldLanes);
        this->newLanes = cloneAnimationLanes(newLanes);
        animationRoot_ = std::move(animationRoot);
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AnimationKeyframeChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.project.AnimationKeyframeChangeAction"));
    }
    AnimationKeyframeChangeAction(const AnimationKeyframeChangeAction&) = delete;
    AnimationKeyframeChangeAction& operator=(const AnimationKeyframeChangeAction&) = delete;
    AnimationKeyframeChangeAction(AnimationKeyframeChangeAction&&) = delete;
    AnimationKeyframeChangeAction& operator=(AnimationKeyframeChangeAction&&) = delete;
    void rollback() override {
        if (!applied) return;
        const auto retainedSelf = shared_from_this();
        restoreAnimationLanes(animation,oldLanes); notify(); applied = false;
    }
    void redo() override {
        if (applied) return;
        const auto retainedSelf = shared_from_this();
        restoreAnimationLanes(animation,newLanes); notify(); applied = true;
    }
    actions::ActionText describe() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Animation keyframe was edited"));
    }
    virtual compat::RetainedString toString() {
        const auto keepAlive = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    actions::ActionText describeUndo() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Undo animation keyframe edit"));
    }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("AnimationKeyframeChange"); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
private:
    std::shared_ptr<Animation> animationRoot_;
    void notify() {
        if (playback) compat::ngRequireObject(incActiveProject())->AnimationChanged.emit(playback);
    }
};
bool animationKeyframeAddRaw(Lanes& lanes, std::shared_ptr<Parameter>& parameter, int axis, float value, int frameNo) {
    auto currentLanes = lanes;
    for (auto& lane : currentLanes) {
        if (nicxlive::core::ngResourceObjectEquals(
                compat::ngRequireObject(lane.paramRef)->targetParam, parameter) &&
                axis == compat::ngRequireObject(lane.paramRef)->targetAxis) {
            auto currentFrames = lane.frames;
            for (auto& frame : currentFrames) if (frame.frame == frameNo) {
                if (frame.value == value) return false;
                frame.value = value; return true;
            }
            lane.frames.push_back(nicxlive::core::Keyframe{frameNo,value,.5f});
            lane.updateFrames(); return true;
        }
    }
    const auto uuid = compat::ngRequireObject(parameter)->getUUID();
    const auto reference = std::allocate_shared<nicxlive::core::AnimationParameterRef>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::AnimationParameterRef>{},
        nicxlive::core::AnimationParameterRef{parameter, axis});
    const auto frames = nicxlive::core::common::SharedArray<nicxlive::core::Keyframe>{
        nicxlive::core::Keyframe{frameNo, value, .5f}};
    const auto lane = nicxlive::core::AnimationLane(
        uuid, reference, frames, nicxlive::core::common::InterpolateMode::Linear);
    lanes.push_back(lane);
    return true;
}
bool animationKeyframeRemoveRaw(Lanes& lanes, std::shared_ptr<Parameter>& parameter, int axis, int frameNo) {
    auto currentLanes = lanes;
    for (auto& lane : currentLanes) {
        if (nicxlive::core::ngResourceObjectEquals(
                compat::ngRequireObject(lane.paramRef)->targetParam, parameter)
            && axis == compat::ngRequireObject(lane.paramRef)->targetAxis) {
            const auto count = lane.frames.size();
            for (std::size_t index = 0; index < count; ++index) {
                if (lane.frames[index].frame == frameNo) {
                    lane.frames = lane.frames.removed(index);
                    return true;
                }
            }
        }
    }
    return false;
}
}
} // namespace nijigenerate::project

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::project::AnimationKeyframeChangeAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.project.AnimationKeyframeChangeAction)";
    inline static constexpr char unqualifiedArgument[] = "(AnimationKeyframeChangeAction)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.project.AnimationKeyframeChangeAction";
    inline static constexpr char unqualifiedElement[] = "AnimationKeyframeChangeAction";
};
template<> struct SourceObjectTypeNames<::nijigenerate::project::AnimationKeyframeChangeAction> {
    inline static constexpr char className[] = "nijigenerate.project.AnimationKeyframeChangeAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.project.AnimationKeyframeChangeAction)";
};
}

namespace nijigenerate::project {
void incAnimationKeyframeAdd(std::shared_ptr<Parameter>& parameter, int axis, float value) {
    auto playback = incAnimationGet();
    if (!playback || !playback->animation() || !parameter) return;
    if (axis < 0 || axis >= 2) return;
    auto animation = playback->animation();
    auto oldLanes = cloneAnimationLanes(animation->lanes);
    auto newLanes = cloneAnimationLanes(oldLanes);
    if (!animationKeyframeAddRaw(newLanes, parameter, axis, value,
            compat::ngRequireObject(incAnimationGet())->frame())) return;
    const auto animationRoot = std::shared_ptr<Animation>(playback, animation);
    auto action = detail::ngCreateProjectAction<AnimationKeyframeChangeAction>([&] {
        return std::tuple{incAnimationGet(), animation, oldLanes, newLanes, animationRoot};
    });
    action->redo(); core::actionstack::incActionPush(action);
}
bool incAnimationKeyframeRemove(std::shared_ptr<Parameter>& parameter, int axis) {
    if (!incAnimationGet() || !compat::ngRequireObject(incAnimationGet())->animation() || !parameter)
        return false;
    if (axis < 0 || axis >= 2) return false;
    const auto playback = compat::ngRequireObject(incAnimationGet());
    auto animation = playback->animation();
    auto oldLanes = cloneAnimationLanes(compat::ngRequireObject(animation)->lanes);
    auto newLanes = cloneAnimationLanes(oldLanes);
    if (!animationKeyframeRemoveRaw(newLanes, parameter, axis,
            compat::ngRequireObject(incAnimationGet())->frame())) return false;
    // This is the real arena owner captured when this exact animation pointer
    // was obtained. The current playback may have changed during Object.equals.
    const auto animationRoot = std::shared_ptr<Animation>(playback, animation);
    auto action = detail::ngCreateProjectAction<AnimationKeyframeChangeAction>([&] {
        return std::tuple{incAnimationGet(), animation, oldLanes, newLanes, animationRoot};
    });
    action->redo(); core::actionstack::incActionPush(action); return true;
}
}
