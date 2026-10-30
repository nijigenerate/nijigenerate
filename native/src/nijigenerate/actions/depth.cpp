#include "depth.hpp"
#include "depthbone_publishers.hpp"
#include "../ext/nodes/exdepthmapped.hpp"
#include <nodes/node.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::actions::depth {
using nicxlive::core::nodes::NotifyReason;

namespace {
template<class T> void prepareDepthActionOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
}

void prepareDepthActionClassInfo() {
    static const bool registered = [] {
        prepareDepthActionOrigin<DepthMappedChangeAction>();
        prepareDepthActionOrigin<DepthOperationMappedChangeAction>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(DepthMappedChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depth.DepthMappedChangeAction"));
        ngRegisterObjectClassName(typeid(DepthOperationMappedChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depth.DepthOperationMappedChangeAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool depthActionClassInfo = [] {
    prepareDepthActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DepthMappedChangeAction::prepareSourceClassInfo() {
    prepareDepthActionClassInfo();
}
compat::RetainedString DepthMappedChangeAction::toString() {
    const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
void DepthOperationMappedChangeAction::prepareSourceClassInfo() {
    prepareDepthActionClassInfo();
}
compat::RetainedString DepthOperationMappedChangeAction::toString() {
    const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

compat::Slice<float> DepthMappedChangeAction::capture() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return compat::ngRequireObject(depthMapped)->copyDepths();
}

void DepthMappedChangeAction::apply(compat::Slice<float> depths) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::ngRequireObject(depthMapped)->replaceDepths(depths);
    compat::ngRequireObject(node)->notifyChange(node, NotifyReason::AttributeChanged);
    depthboneinvalidation::ngNotifyDepthBoneTargetChanged(
        node, depthboneinvalidation::DepthBoneMutationKind::TargetDepth, reason);
}

void DepthMappedChangeAction::updateNewState() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    newDepths = capture();
}

void DepthMappedChangeAction::clear() {}

void DepthMappedChangeAction::rollback() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    apply(oldDepths);
}

void DepthMappedChangeAction::redo() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    apply(newDepths);
}

ActionText DepthMappedChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}

bool DepthMappedChangeAction::merge(ActionPtr) { return false; }
bool DepthMappedChangeAction::canMerge(ActionPtr) { return false; }

DepthOperationMappedChangeAction::State DepthOperationMappedChangeAction::capture() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto operations = compat::ngRequireObject(depthOperated)->copyDepthOps();
    const auto baseDepths = compat::ngRequireObject(depthOperated)->copyDepthOpBaseDepths();
    return State{operations, baseDepths};
}

void DepthOperationMappedChangeAction::apply(State state) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::ngRequireObject(depthOperated)->replaceDepthOps(state.operations);
    compat::ngRequireObject(depthOperated)->replaceDepthOpBaseDepths(state.baseDepths);
    compat::ngRequireObject(node)->notifyChange(node, NotifyReason::AttributeChanged);
}

void DepthOperationMappedChangeAction::updateNewState() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    newState = capture();
}

void DepthOperationMappedChangeAction::clear() {}

void DepthOperationMappedChangeAction::rollback() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    apply(oldState);
}

void DepthOperationMappedChangeAction::redo() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    apply(newState);
}

ActionText DepthOperationMappedChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}

bool DepthOperationMappedChangeAction::merge(ActionPtr) { return false; }
bool DepthOperationMappedChangeAction::canMerge(ActionPtr) { return false; }

std::shared_ptr<DepthOperationMappedChangeAction> ngClearDepthOperationsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> node) {
    auto depthOperated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(node);
    if (!depthOperated) return nullptr;
    if (depthOperated->copyDepthOps().size() == 0 && depthOperated->copyDepthOpBaseDepths().size() == 0)
        return nullptr;

    auto action = DepthOperationMappedChangeAction::createSource([&] { return std::tuple{node}; });
    depthOperated->replaceDepthOps({});
    depthOperated->replaceDepthOpBaseDepths({});
    compat::ngRequireObject(node)->notifyChange(node, NotifyReason::AttributeChanged);
    action->updateNewState();
    return action;
}

} // namespace nijigenerate::actions::depth
