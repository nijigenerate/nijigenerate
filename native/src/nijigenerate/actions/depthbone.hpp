#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include "package.hpp"
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <type_traits>
#include <utility>

namespace nijigenerate::actions::depthbone {
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthRigBinding;
using nicxlive::core::nodes::Node;
using nicxlive::core::math::Vec3;
using Bindings = compat::Slice<ExDepthRigBinding>;
Bindings ngCopyDepthRigBindings(Bindings bindings);

class DepthBoneConstraintChangeAction;
template<class I = DepthBoneConstraintChangeAction>
std::shared_ptr<I> ngCreateDepthBoneConstraintAction(std::shared_ptr<ExDepthBone> bone);

class DepthRigBindingsChangeAction : public Action,
    public std::enable_shared_from_this<DepthRigBindingsChangeAction> {
public:
    DepthRigBindingsChangeAction(const DepthRigBindingsChangeAction&) = delete;
    DepthRigBindingsChangeAction& operator=(const DepthRigBindingsChangeAction&) = delete;
    std::shared_ptr<ExDepthRigRoot> root;
    Bindings oldBindings, newBindings;
    ActionText label;
    bool settleBeforeDispatch = false;
    std::shared_ptr<Node> affectedTarget;
    std::uint64_t mergeSession = 0, mergeSourceUuid = 0;
    ActionText mergeProperty;
    DepthRigBindingsChangeAction(ActionText label, std::shared_ptr<ExDepthRigRoot> root,
        Bindings oldBindings, Bindings newBindings, bool settleBeforeDispatch = false,
        std::shared_ptr<Node> affectedTarget = {}, std::uint64_t mergeSession = 0,
        std::uint64_t mergeSourceUuid = 0, ActionText mergeProperty = {});
    void rollback() override;
    void redo() override;
    ActionText describe() override { return label; }
    ActionText describeUndo() override { return label; }
    ActionText getName() override { return label; }
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    virtual compat::RetainedString toString();
private:
    void notifyChanged();
};
using DepthBoneSourceListChangeAction = DepthRigBindingsChangeAction;
using DepthBoneBindingRuleChangeAction = DepthRigBindingsChangeAction;

class DepthBoneFitZChangeAction : public Action,
    public std::enable_shared_from_this<DepthBoneFitZChangeAction> {
public:
    DepthBoneFitZChangeAction(const DepthBoneFitZChangeAction&) = delete;
    DepthBoneFitZChangeAction& operator=(const DepthBoneFitZChangeAction&) = delete;
    std::shared_ptr<ExDepthRigRoot> root;
    compat::Slice<std::shared_ptr<ExDepthBone>> bones;
    compat::Slice<float> oldValues, newValues;
    DepthBoneFitZChangeAction(std::shared_ptr<ExDepthRigRoot> root,
        compat::Slice<std::shared_ptr<ExDepthBone>> bones,
        compat::Slice<float> oldValues, compat::Slice<float> newValues);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
private:
    void notifyChanged();
    void apply(compat::Slice<float> values);
};

class DepthBoneRestChangeAction : public Action,
    public std::enable_shared_from_this<DepthBoneRestChangeAction> {
public:
    DepthBoneRestChangeAction(const DepthBoneRestChangeAction&) = delete;
    DepthBoneRestChangeAction& operator=(const DepthBoneRestChangeAction&) = delete;
    std::shared_ptr<ExDepthBone> bone;
    Vec3 oldHead{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    Vec3 oldTail{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    float oldRoll = std::numeric_limits<float>::quiet_NaN();
    Vec3 newHead{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    Vec3 newTail{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    float newRoll = std::numeric_limits<float>::quiet_NaN();
    DepthBoneRestChangeAction(std::shared_ptr<ExDepthBone> bone, Vec3 oldHead, Vec3 oldTail,
        float oldRoll, Vec3 newHead, Vec3 newTail, float newRoll);
    virtual void apply(Vec3 head, Vec3 tail, float roll);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
private:
    void notifyChanged();
};

class DepthBoneConstraintChangeAction : public Action,
    public std::enable_shared_from_this<DepthBoneConstraintChangeAction> {
    template<class I> friend std::shared_ptr<I> ngCreateDepthBoneConstraintAction(std::shared_ptr<ExDepthBone>);
protected:
    // Field initialization only; source construction runs after dynamic allocation.
    DepthBoneConstraintChangeAction();
public:
    DepthBoneConstraintChangeAction(const DepthBoneConstraintChangeAction&) = delete;
    DepthBoneConstraintChangeAction& operator=(const DepthBoneConstraintChangeAction&) = delete;
    std::shared_ptr<ExDepthBone> bone;
    compat::RetainedString oldConstraintType;
    Vec3 oldHingeAxis{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    bool oldLockRotation = false, oldLockTranslation = false, oldAllowParentToTargets = false;
    compat::Slice<float> oldRotationLimits;
    float oldMaxStepRadians = std::numeric_limits<float>::quiet_NaN();
    compat::RetainedString newConstraintType;
    Vec3 newHingeAxis{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    bool newLockRotation = false, newLockTranslation = false, newAllowParentToTargets = false;
    compat::Slice<float> newRotationLimits;
    float newMaxStepRadians = std::numeric_limits<float>::quiet_NaN();
    // Use ngCreateDepthBoneConstraintAction so captureOld sees the complete dynamic object.
    virtual void captureOld();
    virtual void updateNewState();
    virtual void apply(compat::RetainedString constraintType, Vec3 hingeAxis, bool lockRotation,
        bool lockTranslation, bool allowParentToTargets, compat::Slice<float> rotationLimits,
        float maxStepRadians);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
private:
    void initialize(std::shared_ptr<ExDepthBone> bone);
    void notifyChanged();
};

template<class I>
std::shared_ptr<I> ngCreateDepthBoneConstraintAction(std::shared_ptr<ExDepthBone> bone) {
    static_assert(std::is_base_of_v<DepthBoneConstraintChangeAction, I>);
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<I>;
    auto* storage = Allocator{}.allocate(1);
    I* instance;
    try {
        instance = ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        Allocator{}.deallocate(storage, 1);
        throw;
    }
    auto action = std::shared_ptr<I>(instance, [](I* value) noexcept {
        std::destroy_at(value);
        Allocator{}.deallocate(value, 1);
    }, Allocator{});
    static_cast<DepthBoneConstraintChangeAction&>(*action).initialize(std::move(bone));
    return action;
}
} // namespace nijigenerate::actions::depthbone

namespace nicxlive::core::d_runtime {
// These nominal origins come from the four defining D declarations. The two
// binding-rule aliases above denote the same class and need no duplicate row.
#define NG_DEPTHBONE_ACTION_TYPE_SPELLINGS(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::actions::depthbone::Type> { \
        inline static constexpr char className[]="nijigenerate.actions.depthbone." #Type; \
        inline static constexpr char templateArgumentName[]="(nijigenerate.actions.depthbone." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::depthbone::Type> { \
        inline static constexpr bool isNativeObject = true; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(nijigenerate.actions.depthbone." #Type ")"), \
                RetainedString::fromStaticLiteral("(" #Type ")"), \
                RetainedString::fromStaticLiteral("nijigenerate.actions.depthbone." #Type), \
                RetainedString::fromStaticLiteral(#Type)}; \
        } \
    }
NG_DEPTHBONE_ACTION_TYPE_SPELLINGS(DepthRigBindingsChangeAction);
NG_DEPTHBONE_ACTION_TYPE_SPELLINGS(DepthBoneFitZChangeAction);
NG_DEPTHBONE_ACTION_TYPE_SPELLINGS(DepthBoneRestChangeAction);
NG_DEPTHBONE_ACTION_TYPE_SPELLINGS(DepthBoneConstraintChangeAction);
#undef NG_DEPTHBONE_ACTION_TYPE_SPELLINGS
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::depthbone::DepthRigBindingsChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::depthbone::DepthRigBindingsChangeAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::depthbone::DepthBoneFitZChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::depthbone::DepthBoneFitZChangeAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::depthbone::DepthBoneRestChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::depthbone::DepthBoneRestChangeAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::depthbone::DepthBoneConstraintChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::depthbone::DepthBoneConstraintChangeAction>> {};

} // namespace nijigenerate::core::translation
