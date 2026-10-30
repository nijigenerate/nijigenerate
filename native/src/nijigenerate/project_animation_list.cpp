#include <nijigenerate/project.hpp>
#include <nijigenerate/project_animation_detail.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::project {
namespace {
using Animation = nicxlive::core::Animation;
using AnimationMap = nicxlive::core::AnimationMap;
using Text = compat::RetainedString;
Animation cloneAnimation(Animation animation) {
    return detail::ngCloneAnimation(animation);
}
AnimationMap cloneAnimations(AnimationMap animations) {
    AnimationMap result;
    for (auto entry = animations.begin(); entry != animations.end(); ++entry) {
        const auto entryOwner = animations.retain(entry);
        const auto& name = entry->first;
        const auto animation = *entryOwner;
        result[name] = cloneAnimation(animation);
    }
    return result;
}
void restoreAnimations(std::shared_ptr<Puppet> puppet, AnimationMap animations, Text currentName) {
    const auto restored = cloneAnimations(animations);
    compat::ngRequireObject(puppet)->getAnimations() = restored;
    incInitAnimationPlayer(puppet);
    if (!currentName.empty()) {
        const auto values = compat::ngRequireObject(puppet)->getAnimations();
        if (values.find(currentName) != values.end()) {
            incAnimationChange(currentName);
            return;
        }
    }
    const auto project = compat::ngRequireObject(incActiveProject());
    project->AnimationChanged.emit(nullptr);
}
Text currentAnimationName() {
    const auto playback = incAnimationGet();
    if (!playback) return Text::emptyLiteral();
    const auto& name = playback->name();
    // A zero-length D .dup returns null rather than allocating an empty owner.
    if (name.empty()) return {};
    return detail::ngDuplicateProjectText(name.view());
}
class AnimationListChangeAction : public actions::Action,
                                  public std::enable_shared_from_this<AnimationListChangeAction> {
public:
    std::shared_ptr<Puppet> puppet;
    AnimationMap oldAnimations, newAnimations;
    Text oldCurrentName, newCurrentName, label;
    bool applied = false;
    AnimationListChangeAction(const AnimationListChangeAction&) = delete;
    AnimationListChangeAction& operator=(const AnimationListChangeAction&) = delete;
    AnimationListChangeAction(AnimationListChangeAction&&) = delete;
    AnimationListChangeAction& operator=(AnimationListChangeAction&&) = delete;
    AnimationListChangeAction() = default;
    void initialize(std::shared_ptr<Puppet> puppet, AnimationMap oldAnimations,
        AnimationMap newAnimations, Text oldCurrentName, Text newCurrentName, Text label) {
        this->puppet = std::move(puppet);
        this->oldAnimations = cloneAnimations(oldAnimations);
        this->newAnimations = cloneAnimations(newAnimations);
        this->oldCurrentName = detail::ngDuplicateProjectText(oldCurrentName.view());
        this->newCurrentName = detail::ngDuplicateProjectText(newCurrentName.view());
        this->label = detail::ngDuplicateProjectText(label.view());
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AnimationListChangeAction),
            Text::fromStaticLiteral("nijigenerate.project.AnimationListChangeAction"));
    }
    void rollback() override {
        if (!applied) return;
        const auto keepAlive = shared_from_this();
        restoreAnimations(puppet, oldAnimations, oldCurrentName); applied = false;
    }
    void redo() override {
        if (applied) return;
        const auto keepAlive = shared_from_this();
        restoreAnimations(puppet, newAnimations, newCurrentName); applied = true;
    }
    actions::ActionText describe() override { return label; }
    virtual Text toString() {
        const auto keepAlive = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    actions::ActionText describeUndo() override {
        using namespace core::translation;
        const auto keepAlive = shared_from_this();
        const auto pattern = ngTranslateRetained(Text::fromStaticLiteral("Undo %s"));
        const std::array<TypedFormatArgument, 1> args{ngFormatArgument(label)};
        return detail::ngRetainProjectText(ngFormatTyped(pattern.view(), args));
    }
    actions::ActionText getName() override { return Text::fromStaticLiteral("AnimationListChange"); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
};
}
} // namespace nijigenerate::project

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::project::AnimationListChangeAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.project.AnimationListChangeAction)";
    inline static constexpr char unqualifiedArgument[] = "(AnimationListChangeAction)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.project.AnimationListChangeAction";
    inline static constexpr char unqualifiedElement[] = "AnimationListChangeAction";
};
template<> struct SourceObjectTypeNames<::nijigenerate::project::AnimationListChangeAction> {
    inline static constexpr char className[] = "nijigenerate.project.AnimationListChangeAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.project.AnimationListChangeAction)";
};
}

namespace nijigenerate::project {
bool ngAnimationCreateOrUpdate(compat::RetainedStringSlice name, Animation animation,
                              compat::RetainedStringSlice originalName) {
    const auto strippedName = compat::ngStripUnicode(name.view());
    const auto nameOffset = name.data() ? static_cast<std::size_t>(strippedName.data() - name.data()) : 0;
    name = name.slice(nameOffset, nameOffset + strippedName.size());
    const auto strippedOriginalName = compat::ngStripUnicode(originalName.view());
    const auto originalOffset = originalName.data()
        ? static_cast<std::size_t>(strippedOriginalName.data() - originalName.data()) : 0;
    originalName = originalName.slice(originalOffset, originalOffset + strippedOriginalName.size());
    if (name.empty()) return false;
    const auto puppet = incActivePuppet();
    auto oldAnimations = cloneAnimations(puppet->getAnimations());
    auto newAnimations = cloneAnimations(oldAnimations);
    const bool rename = !originalName.empty() && originalName.view() != name.view();
    if (rename) newAnimations.erase(originalName.retainedString());
    newAnimations[name.retainedString()] = cloneAnimation(animation);
    auto oldCurrentName = currentAnimationName();
    auto action = detail::ngCreateProjectAction<AnimationListChangeAction>([&] {
        const auto currentName = name.retainedString();
        const auto label = core::translation::ngTranslateRetained(rename
            ? Text::fromStaticLiteral("Rename Animation") : Text::fromStaticLiteral("Save Animation"));
        return std::tuple{puppet, oldAnimations, newAnimations, oldCurrentName, currentName, label};
    });
    action->redo(); core::actionstack::incActionPush(action); return true;
}
bool ngAnimationDelete(compat::RetainedStringSlice name) {
    {
        const auto animations = compat::ngRequireObject(incActivePuppet())->getAnimations();
        if (animations.find(name.retainedString()) == animations.end()) return false;
    }
    const auto puppet = incActivePuppet();
    auto oldAnimations = cloneAnimations(compat::ngRequireObject(puppet)->getAnimations());
    auto newAnimations = cloneAnimations(oldAnimations);
    newAnimations.erase(name.retainedString());
    auto oldName = currentAnimationName();
    auto newName = oldName.view() == name.view() ? Text::emptyLiteral() : oldName;
    auto action = detail::ngCreateProjectAction<AnimationListChangeAction>([&] {
        const auto label = core::translation::ngTranslateRetained(Text::fromStaticLiteral("Delete Animation"));
        return std::tuple{puppet, oldAnimations, newAnimations, oldName, newName, label};
    });
    action->redo(); core::actionstack::incActionPush(action); return true;
}
}
